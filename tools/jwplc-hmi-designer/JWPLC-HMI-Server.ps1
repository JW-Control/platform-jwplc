param(
    [Parameter(Mandatory = $true)]
    [string]$Root,

    [int]$Port = 8765,

    [int]$IdleTimeoutMinutes = 60
)

$ErrorActionPreference = 'Stop'

function Get-ContentType([string]$Path) {
    switch ([IO.Path]::GetExtension($Path).ToLowerInvariant()) {
        '.html' { return 'text/html; charset=utf-8' }
        '.js' { return 'text/javascript; charset=utf-8' }
        '.css' { return 'text/css; charset=utf-8' }
        '.json' { return 'application/json; charset=utf-8' }
        '.webmanifest' { return 'application/manifest+json; charset=utf-8' }
        '.svg' { return 'image/svg+xml' }
        '.png' { return 'image/png' }
        '.ico' { return 'image/x-icon' }
        default { return 'application/octet-stream' }
    }
}

function Write-HttpResponse {
    param(
        [System.IO.Stream]$Stream,
        [int]$StatusCode,
        [string]$StatusText,
        [byte[]]$Body,
        [string]$ContentType = 'text/plain; charset=utf-8',
        [bool]$HeadOnly = $false
    )

    if ($null -eq $Body) { $Body = [byte[]]@() }
    $header = "HTTP/1.1 $StatusCode $StatusText`r`n" +
              "Content-Type: $ContentType`r`n" +
              "Content-Length: $($Body.Length)`r`n" +
              "Cache-Control: no-cache`r`n" +
              "Connection: close`r`n`r`n"
    $headerBytes = [Text.Encoding]::ASCII.GetBytes($header)
    $Stream.Write($headerBytes, 0, $headerBytes.Length)
    if (-not $HeadOnly -and $Body.Length -gt 0) {
        $Stream.Write($Body, 0, $Body.Length)
    }
    $Stream.Flush()
}

$rootFull = [IO.Path]::GetFullPath($Root)
if (-not (Test-Path -LiteralPath $rootFull -PathType Container)) {
    throw "No existe la raíz del Designer: $rootFull"
}

$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $Port)
try {
    $listener.Start()
} catch {
    # Otro launcher puede haber dejado el servidor activo. El proceso nuevo no
    # debe competir por el mismo puerto.
    exit 0
}

$lastRequest = [DateTime]::UtcNow
$idleTimeout = [TimeSpan]::FromMinutes([Math]::Max(5, $IdleTimeoutMinutes))

try {
    while ($true) {
        if (-not $listener.Pending()) {
            if (([DateTime]::UtcNow - $lastRequest) -ge $idleTimeout) { break }
            Start-Sleep -Milliseconds 120
            continue
        }

        $client = $listener.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::ASCII, $false, 4096, $true)
            $requestLine = $reader.ReadLine()
            if ([string]::IsNullOrWhiteSpace($requestLine)) { continue }

            do {
                $line = $reader.ReadLine()
            } while ($null -ne $line -and $line.Length -gt 0)

            $parts = $requestLine.Split(' ')
            if ($parts.Length -lt 2) { continue }
            $method = $parts[0].ToUpperInvariant()
            if ($method -ne 'GET' -and $method -ne 'HEAD') {
                $body = [Text.Encoding]::UTF8.GetBytes('Method Not Allowed')
                Write-HttpResponse -Stream $stream -StatusCode 405 -StatusText 'Method Not Allowed' -Body $body -HeadOnly ($method -eq 'HEAD')
                continue
            }

            $requestTarget = $parts[1]
            $pathOnly = $requestTarget.Split('?')[0]
            $decoded = [Uri]::UnescapeDataString($pathOnly)
            if ($decoded -eq '/__health') {
                $body = [Text.Encoding]::UTF8.GetBytes('JWPLC_HMI_OK')
                Write-HttpResponse -Stream $stream -StatusCode 200 -StatusText 'OK' -Body $body -HeadOnly ($method -eq 'HEAD')
                $lastRequest = [DateTime]::UtcNow
                continue
            }

            if ($decoded -eq '/' -or [string]::IsNullOrWhiteSpace($decoded)) {
                $decoded = '/desktop.html'
            }

            $relative = $decoded.TrimStart('/').Replace('/', [IO.Path]::DirectorySeparatorChar)
            $candidate = [IO.Path]::GetFullPath((Join-Path $rootFull $relative))
            $rootPrefix = $rootFull.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar

            if (-not $candidate.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -and
                -not $candidate.Equals($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
                $body = [Text.Encoding]::UTF8.GetBytes('Forbidden')
                Write-HttpResponse -Stream $stream -StatusCode 403 -StatusText 'Forbidden' -Body $body -HeadOnly ($method -eq 'HEAD')
                continue
            }

            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                $body = [Text.Encoding]::UTF8.GetBytes('Not Found')
                Write-HttpResponse -Stream $stream -StatusCode 404 -StatusText 'Not Found' -Body $body -HeadOnly ($method -eq 'HEAD')
                continue
            }

            $body = [IO.File]::ReadAllBytes($candidate)
            Write-HttpResponse -Stream $stream -StatusCode 200 -StatusText 'OK' -Body $body -ContentType (Get-ContentType $candidate) -HeadOnly ($method -eq 'HEAD')
            $lastRequest = [DateTime]::UtcNow
        } catch {
            try {
                if ($stream) {
                    $body = [Text.Encoding]::UTF8.GetBytes('Internal Server Error')
                    Write-HttpResponse -Stream $stream -StatusCode 500 -StatusText 'Internal Server Error' -Body $body
                }
            } catch {}
        } finally {
            try { $client.Close() } catch {}
        }
    }
} finally {
    try { $listener.Stop() } catch {}
}
