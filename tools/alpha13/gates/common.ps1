Set-StrictMode -Version Latest

function Get-A13Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash($stream)
    }
    finally {
        $stream.Dispose()
        $sha256.Dispose()
    }

    return ([System.BitConverter]::ToString($hash)).Replace('-', '').ToLowerInvariant()
}

function Invoke-A13NativeCaptured {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $oldPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(
            & $FilePath @Arguments 2>&1 |
                ForEach-Object { $_.ToString() }
        )
        $exitCode = [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $oldPreference
    }

    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output   = $output
    }
}

function Get-A13LogValue {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $pattern = '(?m)^' + [regex]::Escape($Key) + '=(.*)\r?$'
    $matches = @([regex]::Matches($Text, $pattern))

    if ($matches.Count -ne 1) {
        return $null
    }

    return $matches[0].Groups[1].Value.Trim()
}

function Get-A13LogInt {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $value = Get-A13LogValue -Text $Text -Key $Key
    if ($null -eq $value) {
        return $null
    }

    $parsed = 0L
    if (-not [int64]::TryParse($value, [ref]$parsed)) {
        return $null
    }

    return $parsed
}

function Get-A13TrackedDirty {
    return @(git diff --name-only | Where-Object { $_ -and $_.Trim().Length -gt 0 })
}

function Get-A13Staged {
    return @(git diff --cached --name-only | Where-Object { $_ -and $_.Trim().Length -gt 0 })
}

function Get-A13SerialPorts {
    return @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique)
}

function Resolve-A13SerialPort {
    param([string]$RequestedPort = '')

    $ports = @(Get-A13SerialPorts)
    $requested = $RequestedPort.Trim()

    if ($requested.Length -gt 0) {
        if ($requested -in $ports) {
            return [PSCustomObject]@{
                Status     = 'PASS'
                Port       = $requested
                Ports      = $ports
                Candidates = @($requested)
                Reason     = 'EXPLICIT_PORT_PRESENT'
            }
        }

        return [PSCustomObject]@{
            Status     = 'REVIEW_ENVIRONMENT'
            Port       = $null
            Ports      = $ports
            Candidates = @()
            Reason     = 'EXPLICIT_PORT_NOT_PRESENT'
        }
    }

    # COM1 is commonly the built-in/legacy Windows serial port. Never select it
    # implicitly for a JWPLC upload. An explicit -SerialPort COM1 still works.
    $candidates = @($ports | Where-Object { $_ -ine 'COM1' })

    if ($candidates.Count -eq 1) {
        return [PSCustomObject]@{
            Status     = 'PASS'
            Port       = $candidates[0]
            Ports      = $ports
            Candidates = $candidates
            Reason     = 'UNIQUE_NON_COM1_CANDIDATE'
        }
    }

    $reason = if ($candidates.Count -eq 0) {
        'NO_SAFE_SERIAL_CANDIDATE'
    }
    else {
        'AMBIGUOUS_SERIAL_CANDIDATES'
    }

    return [PSCustomObject]@{
        Status     = 'REVIEW_ENVIRONMENT'
        Port       = $null
        Ports      = $ports
        Candidates = $candidates
        Reason     = $reason
    }
}
