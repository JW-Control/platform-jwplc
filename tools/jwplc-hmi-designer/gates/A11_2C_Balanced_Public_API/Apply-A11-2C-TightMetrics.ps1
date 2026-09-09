param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
)

$ErrorActionPreference = "Stop"

$uiPath = Join-Path $RepoRoot "JWPLC\2.1.0\libraries\JWPLC_Display\src\JWPLC_UI.cpp"
if (-not (Test-Path -LiteralPath $uiPath)) {
    throw "No se encontro JWPLC_UI.cpp en: $uiPath"
}

$text = [System.IO.File]::ReadAllText($uiPath)
$original = $text

function Replace-ExactlyOnce {
    param(
        [string]$Name,
        [string]$Old,
        [string]$New
    )

    $count = ([regex]::Matches($script:text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "[$Name] Se esperaba 1 coincidencia y se encontraron $count. No se aplico el parche."
    }

    $script:text = $script:text.Replace($Old, $New)
}

Replace-ExactlyOnce -Name "tight-text-bounds" -Old @'
        tft.setTextSize((textSize == 0) ? 1 : textSize);
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    }
'@ -New @'
        const uint8_t scale = (textSize == 0) ? 1 : textSize;
        tft.setTextSize(scale);
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

        // HMI declarativa: Adafruit GFX reporta la celda clasica 6x8 completa.
        // Para el layout del field usamos el cuerpo nominal 5x7 y dejamos el
        // spacer final / fila de descenso dentro del padding del contenedor.
        if (w >= scale)
        {
            w -= scale;
        }

        if (h >= scale)
        {
            h -= scale;
        }
    }

    int16_t effectiveFieldPadding(const JWPLC_UIField &def)
    {
        uint8_t maxTextSize = (def.style.labelTextSize == 0)
                                  ? 1
                                  : def.style.labelTextSize;

        if (def.meta.type != JWPLC_UI_FIELD_BAR)
        {
            const uint8_t valueSize = (def.style.valueTextSize == 0)
                                          ? 1
                                          : def.style.valueTextSize;
            if (valueSize > maxTextSize)
            {
                maxTextSize = valueSize;
            }
        }

        // FIELD_PADDING sigue siendo el margen visual minimo. Para escalas
        // mayores reservamos al menos una celda de escala para contener la
        // posible fila 8 (descenders) sin salir del rectangulo del field.
        return (maxTextSize > FIELD_PADDING)
                   ? (int16_t)maxTextSize
                   : FIELD_PADDING;
    }
'@

Replace-ExactlyOnce -Name "compute-pad-early" -Old @'
    void computeFieldGeometry(Adafruit_ST7789 &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;

        uint16_t labelW = 0;
'@ -New @'
    void computeFieldGeometry(Adafruit_ST7789 &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;
        const int16_t pad = effectiveFieldPadding(def);

        uint16_t labelW = 0;
'@

Replace-ExactlyOnce -Name "bar-auto-padding" -Old @'
                    : (uint16_t)((def.rect.width > 2 * FIELD_PADDING)
                                     ? def.rect.width - 2 * FIELD_PADDING
                                     : DEFAULT_BAR_WIDTH);
'@ -New @'
                    : (uint16_t)((def.rect.width > 2 * pad)
                                     ? def.rect.width - 2 * pad
                                     : DEFAULT_BAR_WIDTH);
'@

Replace-ExactlyOnce -Name "bar-fixed-padding" -Old @'
            int16_t available = def.rect.width - 2 * FIELD_PADDING;
'@ -New @'
            int16_t available = def.rect.width - 2 * pad;
'@

Replace-ExactlyOnce -Name "remove-old-pad" -Old @'
        const int16_t pad = FIELD_PADDING;
        int16_t autoW = 0;
'@ -New @'
        int16_t autoW = 0;
'@

Replace-ExactlyOnce -Name "fallback-height" -Old @'
                : (int16_t)((valueH == 0)
                                 ? (6 * def.style.valueTextSize)
                                 : valueH);
'@ -New @'
                : (int16_t)((valueH == 0)
                                 ? (7 * ((def.style.valueTextSize == 0)
                                            ? 1
                                            : def.style.valueTextSize))
                                 : valueH);
'@

Replace-ExactlyOnce -Name "static-pad" -Old @'
        const JWPLC_UIField &def = field.def;

        if (field.fieldW > 0 && field.fieldH > 0)
'@ -New @'
        const JWPLC_UIField &def = field.def;
        const int16_t pad = effectiveFieldPadding(def);

        if (field.fieldW > 0 && field.fieldH > 0)
'@

$cursorOld = @'
            tft.setCursor(
                def.rect.x + FIELD_PADDING,
                def.rect.y + FIELD_PADDING);
'@
$cursorNew = @'
            tft.setCursor(
                def.rect.x + pad,
                def.rect.y + pad);
'@
$countCursor = ([regex]::Matches($text, [regex]::Escape($cursorOld))).Count
if ($countCursor -ne 1) {
    throw "[label-cursor-padding] Se esperaba 1 coincidencia y se encontraron $countCursor."
}
$text = $text.Replace($cursorOld, $cursorNew)

Replace-ExactlyOnce -Name "dirty-clear-height" -Old @'
        // Alpha8: al cambiar una variable solo se limpia su region VALUE.
        tft.fillRect(
            field.valueX,
            field.valueY,
            field.valueW,
            field.valueH,
            def.style.colors.background);
'@ -New @'
        // Al cambiar una variable solo se limpia su region VALUE. El layout
        // usa el cuerpo nominal 5x7, pero la fuente clasica puede usar la fila
        // 8 (descenders), por eso la limpieza vertical cubre una escala extra.
        int16_t clearH = field.valueH;

        if (def.meta.type != JWPLC_UI_FIELD_BAR)
        {
            const int16_t scale = (def.style.valueTextSize == 0)
                                      ? 1
                                      : (int16_t)def.style.valueTextSize;
            clearH += scale;

            const int16_t maxClearH =
                (field.fieldY + field.fieldH) - field.valueY;
            if (clearH > maxClearH)
            {
                clearH = maxClearH;
            }
        }

        tft.fillRect(
            field.valueX,
            field.valueY,
            field.valueW,
            clearH,
            def.style.colors.background);
'@

if ($text -eq $original) {
    throw "No se detectaron cambios."
}

[System.IO.File]::WriteAllText($uiPath, $text, [System.Text.UTF8Encoding]::new($false))

Write-Host "A11-2C: tight HMI text metrics aplicadas." -ForegroundColor Green
Write-Host "Archivo: $uiPath"
Write-Host "Esperado para TEXT size=2 capacity=12 sin label: field AUTO 148x20; margen visual nominal 3 px." -ForegroundColor Cyan
