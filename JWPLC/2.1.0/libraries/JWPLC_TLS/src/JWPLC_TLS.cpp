#include "JWPLC_TLS.h"

#include <JWPLC_Ethernet.h>
#include <new>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// =====================================================================================
// Común
// =====================================================================================

JWPLC_TLSClient::JWPLC_TLSClient() = default;

JWPLC_TLSClient::~JWPLC_TLSClient()
{
    stop();
}

void JWPLC_TLSClient::setCACert(const char pem[])
{
    caPem_ = pem;
    insecure_ = false;
}

void JWPLC_TLSClient::useDefaultCA()
{
    caPem_ = nullptr;
    insecure_ = false;
}

void JWPLC_TLSClient::setInsecure()
{
    insecure_ = true;
}

void JWPLC_TLSClient::setHandshakeTimeout(uint32_t ms)
{
    handshakeTimeoutMs_ = ms;
}

uint8_t JWPLC_TLSClient::lastError()
{
    return error_;
}

const char *JWPLC_TLSClient::errorString()
{
    switch (error_)
    {
    case TLS_OK:
        return "OK";
    case TLS_ERROR_NO_NETWORK:
        return "SIN CONEXION";
    case TLS_ERROR_HANDSHAKE:
        return "SALUDO TLS FALLIDO";
    case TLS_ERROR_CERTIFICATE:
        return "CERTIFICADO NO VALIDO";
    case TLS_ERROR_MEMORY:
        return "SIN MEMORIA";
    case TLS_ERROR_CA:
        return "CA NO VALIDA";
    case TLS_ERROR_CLOSED:
        return "CONEXION CERRADA";
    default:
        return "?";
    }
}

uint8_t JWPLC_TLSClient::network()
{
    return state_ != nullptr ? tcp_.network() : RED_NONE;
}

int JWPLC_TLSClient::connect(const char *host, uint16_t port)
{
    if (host == nullptr || host[0] == '\0')
    {
        error_ = TLS_ERROR_NO_NETWORK;
        return 0;
    }
    return openSecure(host, IPAddress(), true, port);
}

int JWPLC_TLSClient::connect(IPAddress ip, uint16_t port)
{
    return openSecure(nullptr, ip, false, port);
}

int JWPLC_TLSClient::read()
{
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
}

size_t JWPLC_TLSClient::write(uint8_t b)
{
    return write(&b, 1);
}

JWPLC_TLSClient::operator bool()
{
    return connected() != 0;
}

#if defined(JWSIM)
// =====================================================================================
// JWPLC Sim: el cifrado lo hace la PC.
//
// El sketch abre una conexión TCP normal (por el W5500 simulado) al servicio TLS del puente de red
// en la puerta de enlace, puerto 4430, y le pide el destino en una línea:
//     JWSIM-TLS <host> <puerto> <verificar 1|0>\n
// El puente abre TLS hacia el destino con las CA de la PC y responde "OK\n" o "ERR <motivo>\n"; desde
// ahí pasa los datos en claro en los dos sentidos. Así el sketch, la red elegida y el flujo de datos
// son los mismos que en la placa.
// =====================================================================================

namespace
{
const uint16_t kSimTlsPort = 4430;
}

struct JWPLC_TLSState
{
    bool open = false;
};

void JWPLC_TLSClient::freeState()
{
    delete state_;
    state_ = nullptr;
    peeked_ = -1;
}

int JWPLC_TLSClient::openSecure(const char *host, IPAddress ip, bool byName, uint16_t port)
{
    stop();
    error_ = TLS_OK;
    if (!byName && !insecure_)
    {
        error_ = TLS_ERROR_CERTIFICATE; // sin nombre no se puede verificar el certificado
        return 0;
    }
    IPAddress gw = JWPLC_Ethernet.gatewayIP();
    if (!JWPLC_Red.isReady() || gw == IPAddress() || !tcp_.connect(gw, kSimTlsPort))
    {
        error_ = TLS_ERROR_NO_NETWORK;
        return 0;
    }
    String pedido = "JWSIM-TLS ";
    pedido += byName ? String(host) : ip.toString();
    pedido += " ";
    pedido += String(port);
    pedido += insecure_ ? " 0\n" : " 1\n";
    tcp_.print(pedido);

    String respuesta;
    const uint32_t t0 = millis();
    while (millis() - t0 < handshakeTimeoutMs_)
    {
        while (tcp_.available() > 0)
        {
            const int c = tcp_.read();
            if (c == '\n')
            {
                if (respuesta == "OK")
                {
                    state_ = new JWPLC_TLSState();
                    state_->open = true;
                    return 1;
                }
                error_ = respuesta.indexOf("CERT") >= 0 ? TLS_ERROR_CERTIFICATE
                         : respuesta.indexOf("CONNECT") >= 0 ? TLS_ERROR_NO_NETWORK
                                                             : TLS_ERROR_HANDSHAKE;
                tcp_.stop();
                return 0;
            }
            if (respuesta.length() < 120)
                respuesta += (char)c;
        }
        if (!tcp_.connected())
            break;
        vTaskDelay(1);
    }
    error_ = TLS_ERROR_HANDSHAKE;
    tcp_.stop();
    return 0;
}

size_t JWPLC_TLSClient::write(const uint8_t *buf, size_t size)
{
    if (state_ == nullptr)
        return 0;
    const size_t n = tcp_.write(buf, size);
    if (n == 0 && size > 0)
        error_ = TLS_ERROR_CLOSED;
    return n;
}

int JWPLC_TLSClient::available()
{
    if (state_ == nullptr)
        return 0;
    return tcp_.available() + (peeked_ >= 0 ? 1 : 0);
}

int JWPLC_TLSClient::read(uint8_t *buf, size_t size)
{
    if (state_ == nullptr || size == 0)
        return -1;
    size_t n = 0;
    if (peeked_ >= 0)
    {
        buf[n++] = (uint8_t)peeked_;
        peeked_ = -1;
    }
    if (n < size && tcp_.available() > 0)
    {
        const int r = tcp_.read(buf + n, size - n);
        if (r > 0)
            n += (size_t)r;
    }
    return n > 0 ? (int)n : -1;
}

int JWPLC_TLSClient::peek()
{
    if (state_ == nullptr)
        return -1;
    if (peeked_ < 0 && tcp_.available() > 0)
        peeked_ = tcp_.read();
    return peeked_;
}

void JWPLC_TLSClient::flush()
{
    if (state_ != nullptr)
        tcp_.flush();
}

void JWPLC_TLSClient::stop()
{
    if (state_ != nullptr || tcp_.connected())
        tcp_.stop();
    freeState();
}

uint8_t JWPLC_TLSClient::connected()
{
    if (state_ == nullptr)
        return 0;
    return (tcp_.connected() || tcp_.available() > 0 || peeked_ >= 0) ? 1 : 0;
}

#else
// =====================================================================================
// Placa: mbedTLS del ESP-IDF sobre JWPLC_RedClient (cable o WiFi).
// =====================================================================================

#include "JWPLC_TLS_Roots.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

struct JWPLC_TLSState
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt ca;
    bool handshakeDone = false;
};

namespace
{
// El cliente TCP lo da JWPLC_Red. Sin datos todavía → WANT_READ (mbedTLS vuelve a llamar).
int bioSend(void *ctx, const unsigned char *buf, size_t len)
{
    JWPLC_RedClient *tcp = static_cast<JWPLC_RedClient *>(ctx);
    if (!tcp->connected())
        return MBEDTLS_ERR_NET_CONN_RESET;
    const size_t n = tcp->write(buf, len);
    return n > 0 ? (int)n : MBEDTLS_ERR_SSL_WANT_WRITE;
}

int bioRecv(void *ctx, unsigned char *buf, size_t len)
{
    JWPLC_RedClient *tcp = static_cast<JWPLC_RedClient *>(ctx);
    if (tcp->available() <= 0)
        return tcp->connected() ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_CONN_RESET;
    const int n = tcp->read(buf, len);
    return n > 0 ? n : MBEDTLS_ERR_SSL_WANT_READ;
}

bool wantMore(int r)
{
    return r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE;
}
} // namespace

void JWPLC_TLSClient::freeState()
{
    if (state_ != nullptr)
    {
        mbedtls_ssl_free(&state_->ssl);
        mbedtls_ssl_config_free(&state_->conf);
        mbedtls_ctr_drbg_free(&state_->drbg);
        mbedtls_entropy_free(&state_->entropy);
        mbedtls_x509_crt_free(&state_->ca);
        delete state_;
        state_ = nullptr;
    }
    peeked_ = -1;
}

int JWPLC_TLSClient::openSecure(const char *host, IPAddress ip, bool byName, uint16_t port)
{
    stop();
    error_ = TLS_OK;
    if (!byName && !insecure_)
    {
        error_ = TLS_ERROR_CERTIFICATE; // sin nombre no se puede verificar el certificado
        return 0;
    }

    state_ = new (std::nothrow) JWPLC_TLSState();
    if (state_ == nullptr)
    {
        error_ = TLS_ERROR_MEMORY;
        return 0;
    }
    mbedtls_ssl_init(&state_->ssl);
    mbedtls_ssl_config_init(&state_->conf);
    mbedtls_ctr_drbg_init(&state_->drbg);
    mbedtls_entropy_init(&state_->entropy);
    mbedtls_x509_crt_init(&state_->ca);

    static const char kPers[] = "jwplc_tls";
    if (mbedtls_ctr_drbg_seed(&state_->drbg, mbedtls_entropy_func, &state_->entropy,
                              reinterpret_cast<const unsigned char *>(kPers), sizeof(kPers) - 1) != 0 ||
        mbedtls_ssl_config_defaults(&state_->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0)
    {
        error_ = TLS_ERROR_MEMORY;
        freeState();
        return 0;
    }
    mbedtls_ssl_conf_rng(&state_->conf, mbedtls_ctr_drbg_random, &state_->drbg);

    if (insecure_)
    {
        mbedtls_ssl_conf_authmode(&state_->conf, MBEDTLS_SSL_VERIFY_NONE);
    }
    else
    {
        const char *pem = caPem_ != nullptr ? caPem_ : JWPLC_TLS_ROOTS_ISRG;
        if (mbedtls_x509_crt_parse(&state_->ca, reinterpret_cast<const unsigned char *>(pem), strlen(pem) + 1) < 0)
        {
            error_ = TLS_ERROR_CA;
            freeState();
            return 0;
        }
        mbedtls_ssl_conf_ca_chain(&state_->conf, &state_->ca, nullptr);
        mbedtls_ssl_conf_authmode(&state_->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    }

    if (mbedtls_ssl_setup(&state_->ssl, &state_->conf) != 0 ||
        (byName && mbedtls_ssl_set_hostname(&state_->ssl, host) != 0))
    {
        error_ = TLS_ERROR_MEMORY;
        freeState();
        return 0;
    }

    const int ok = byName ? tcp_.connect(host, port) : tcp_.connect(ip, port);
    if (!ok)
    {
        error_ = TLS_ERROR_NO_NETWORK;
        freeState();
        return 0;
    }
    mbedtls_ssl_set_bio(&state_->ssl, &tcp_, bioSend, bioRecv, nullptr);

    const uint32_t t0 = millis();
    int r;
    while ((r = mbedtls_ssl_handshake(&state_->ssl)) != 0)
    {
        if (!wantMore(r) || millis() - t0 >= handshakeTimeoutMs_)
        {
            const uint32_t flags = mbedtls_ssl_get_verify_result(&state_->ssl);
            error_ = (r == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED || (flags != 0 && flags != (uint32_t)-1))
                         ? TLS_ERROR_CERTIFICATE
                     : r == MBEDTLS_ERR_SSL_ALLOC_FAILED ? TLS_ERROR_MEMORY
                                                         : TLS_ERROR_HANDSHAKE;
            tcp_.stop();
            freeState();
            return 0;
        }
        vTaskDelay(1);
    }
    state_->handshakeDone = true;
    return 1;
}

size_t JWPLC_TLSClient::write(const uint8_t *buf, size_t size)
{
    if (state_ == nullptr || !state_->handshakeDone)
        return 0;
    size_t sent = 0;
    const uint32_t t0 = millis();
    while (sent < size)
    {
        const int r = mbedtls_ssl_write(&state_->ssl, buf + sent, size - sent);
        if (r > 0)
        {
            sent += (size_t)r;
            continue;
        }
        if (!wantMore(r) || millis() - t0 >= 5000)
        {
            error_ = TLS_ERROR_CLOSED;
            break;
        }
        vTaskDelay(1);
    }
    return sent;
}

int JWPLC_TLSClient::available()
{
    if (state_ == nullptr || !state_->handshakeDone)
        return 0;
    // Procesa un registro TLS si ya llegó (no espera).
    mbedtls_ssl_read(&state_->ssl, nullptr, 0);
    return (int)mbedtls_ssl_get_bytes_avail(&state_->ssl) + (peeked_ >= 0 ? 1 : 0);
}

int JWPLC_TLSClient::read(uint8_t *buf, size_t size)
{
    if (state_ == nullptr || !state_->handshakeDone || size == 0)
        return -1;
    size_t n = 0;
    if (peeked_ >= 0)
    {
        buf[n++] = (uint8_t)peeked_;
        peeked_ = -1;
        if (n == size)
            return (int)n;
    }
    const int r = mbedtls_ssl_read(&state_->ssl, buf + n, size - n);
    if (r > 0)
        n += (size_t)r;
    else if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_NET_CONN_RESET)
        tcp_.stop(); // el servidor cerró
    return n > 0 ? (int)n : -1;
}

int JWPLC_TLSClient::peek()
{
    if (peeked_ < 0)
    {
        uint8_t b;
        if (available() > 0 && read(&b, 1) == 1)
            peeked_ = b;
    }
    return peeked_;
}

void JWPLC_TLSClient::flush()
{
}

void JWPLC_TLSClient::stop()
{
    if (state_ != nullptr && state_->handshakeDone && tcp_.connected())
        mbedtls_ssl_close_notify(&state_->ssl);
    if (state_ != nullptr || tcp_.connected())
        tcp_.stop();
    freeState();
}

uint8_t JWPLC_TLSClient::connected()
{
    if (state_ == nullptr || !state_->handshakeDone)
        return 0;
    if (peeked_ >= 0 || mbedtls_ssl_get_bytes_avail(&state_->ssl) > 0)
        return 1;
    return tcp_.connected() ? 1 : 0;
}

#endif
