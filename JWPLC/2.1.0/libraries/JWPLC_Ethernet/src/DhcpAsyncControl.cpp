#include <Arduino.h>
#include "JWPLC_W5x00_Ethernet.h"
#include "Dhcp.h"
#include "utility/w5100.h"

static constexpr unsigned long DHCP_MAINT_RETRY_MS = 1000UL;

void DhcpClass::updateLeaseTimers()
{
    const unsigned long now = millis();
    unsigned long elapsed = now - _lastCheckLeaseMillis;

    if (elapsed < 1000)
    {
        return;
    }

    _lastCheckLeaseMillis = now - (elapsed % 1000);
    elapsed /= 1000;

    if (_renewInSec < elapsed * 2)
    {
        _renewInSec = 0;
    }
    else
    {
        _renewInSec -= elapsed;
    }

    if (_rebindInSec < elapsed * 2)
    {
        _rebindInSec = 0;
    }
    else
    {
        _rebindInSec -= elapsed;
    }
}

bool DhcpClass::startLeaseMaintenance(uint8_t mode)
{
    if (mode != LEASE_MAINT_RENEW && mode != LEASE_MAINT_REBIND)
    {
        return false;
    }

    _dhcpUdpSocket.stop();
    if (_dhcpUdpSocket.begin(DHCP_CLIENT_PORT) == 0)
    {
        return false;
    }

    presend_DHCP();

    _dhcpTransactionId = random(1UL, 2000UL);
    _dhcpInitialTransactionId = _dhcpTransactionId;

    if (mode == LEASE_MAINT_REBIND)
    {
        // Preserve upstream semantics: a rebind restarts discovery and clears
        // the cached DHCP addresses before accepting another server.
        reset_DHCP_lease();
        _dhcp_state = STATE_DHCP_START;
    }
    else
    {
        // Renew keeps the current lease data and requests it again.
        _dhcp_state = STATE_DHCP_REREQUEST;
    }

    const unsigned long now = millis();
    _asyncStartTime = now;
    _asyncStateStartTime = now;
    _asyncActive = true;
    _leaseMaintenanceMode = mode;
    _leaseRetryNotBeforeMs = 0;
    return true;
}

int DhcpClass::pollLeaseMaintenance()
{
    updateLeaseTimers();

    if (_leaseMaintenanceMode != LEASE_MAINT_NONE)
    {
        const uint8_t completedMode = _leaseMaintenanceMode;
        const int result = pollDHCP();

        if (result == 0)
        {
            return DHCP_CHECK_NONE;
        }

        _leaseMaintenanceMode = LEASE_MAINT_NONE;

        if (result > 0)
        {
            _leaseRetryNotBeforeMs = 0;
            return (completedMode == LEASE_MAINT_RENEW)
                       ? DHCP_CHECK_RENEW_OK
                       : DHCP_CHECK_REBIND_OK;
        }

        // Keep the previously programmed W5x00 network usable while a retry
        // is scheduled. A failed rebind may have cleared the DhcpClass cache,
        // but EthernetClass does not apply that cache until a later success.
        _dhcp_state = STATE_DHCP_LEASED;
        _leaseRetryNotBeforeMs = millis() + DHCP_MAINT_RETRY_MS;

        if (completedMode == LEASE_MAINT_RENEW)
        {
            _renewInSec = 0;
            return DHCP_CHECK_RENEW_FAIL;
        }

        _rebindInSec = 0;
        return DHCP_CHECK_REBIND_FAIL;
    }

    // Initial asynchronous acquisition is handled by pollDHCP(), not by the
    // lease-maintenance state machine.
    if (_asyncActive || _dhcp_state != STATE_DHCP_LEASED)
    {
        return DHCP_CHECK_NONE;
    }

    const unsigned long now = millis();
    if (_leaseRetryNotBeforeMs != 0 &&
        (int32_t)(now - _leaseRetryNotBeforeMs) < 0)
    {
        return DHCP_CHECK_NONE;
    }
    _leaseRetryNotBeforeMs = 0;

    // T2 has priority over T1. Once rebind is due, do not keep attempting a
    // server-specific renewal first.
    if (_rebindInSec == 0)
    {
        if (!startLeaseMaintenance(LEASE_MAINT_REBIND))
        {
            _leaseRetryNotBeforeMs = now + DHCP_MAINT_RETRY_MS;
            return DHCP_CHECK_REBIND_FAIL;
        }
        return DHCP_CHECK_NONE;
    }

    if (_renewInSec == 0)
    {
        if (!startLeaseMaintenance(LEASE_MAINT_RENEW))
        {
            _leaseRetryNotBeforeMs = now + DHCP_MAINT_RETRY_MS;
            return DHCP_CHECK_RENEW_FAIL;
        }
    }

    return DHCP_CHECK_NONE;
}

int EthernetClass::maintainAsync()
{
    if (_dhcp == NULL)
    {
        return DHCP_CHECK_NONE;
    }

    const int rc = _dhcp->pollLeaseMaintenance();

    if (rc == DHCP_CHECK_RENEW_OK || rc == DHCP_CHECK_REBIND_OK)
    {
        // A successful renew/rebind may change the assigned network data.
        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
        W5100.setIPAddress(_dhcp->getLocalIp().raw_address());
        W5100.setGatewayIp(_dhcp->getGatewayIp().raw_address());
        W5100.setSubnetMask(_dhcp->getSubnetMask().raw_address());
        SPI.endTransaction();
        _dnsServerAddress = _dhcp->getDnsServerIp();
    }

    return rc;
}

bool EthernetClass::dhcpMaintenanceInProgress()
{
    return (_dhcp != NULL) && _dhcp->leaseMaintenanceInProgress();
}

void DhcpClass::cancelDHCP()
{
    _dhcpUdpSocket.stop();
    _asyncActive = false;
    _dhcp_state = STATE_DHCP_START;
    _leaseMaintenanceMode = LEASE_MAINT_NONE;
    _leaseRetryNotBeforeMs = 0;
}

void EthernetClass::cancelDHCP()
{
    if (_dhcp == NULL)
    {
        return;
    }

    _dhcp->cancelDHCP();
    _dhcp = NULL;
}
