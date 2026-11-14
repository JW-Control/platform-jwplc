/**
 * JWPLC Basic - mBlock PoC
 * Gate 2 v2
 *
 * Custom Upload Middleware.
 *
 * Objetivo:
 * comprobar si el entorno de ejecución del middleware
 * expone una API Node compatible con child_process.
 *
 * Este probe:
 * - no compila;
 * - no hace upload;
 * - no modifica hardware;
 * - devuelve siempre el código original.
 */

async (
    app,
    device,
    code,
    params,
    logH,
    progressH
) => {

    const log = (text) => {
        const line = '[JWPLC MBLOCK POC] ' + text;

        try {
            if (typeof logH === 'function') {
                logH(line);
            }
        }
        catch (_) {}

        try {
            if (app && typeof app.log === 'function') {
                app.log(line);
            }
        }
        catch (_) {}

        try {
            console.log(line);
        }
        catch (_) {}
    };

    log('============================================');
    log('GATE2_MIDDLEWARE_INVOKED=YES');

    log(
        'GATE2_CODE_TYPE=' +
        typeof code
    );

    log(
        'GATE2_CODE_LENGTH=' +
        String(
            typeof code === 'string'
                ? code.length
                : -1
        )
    );

    /*
     * Esta API NO forma parte de la documentación pública
     * de Extension Builder.
     *
     * Gate 2 comprueba únicamente si el sandbox la expone.
     */
    const requireAvailable =
        typeof require === 'function';

    log(
        'GATE2_REQUIRE_AVAILABLE=' +
        (
            requireAvailable
                ? 'YES'
                : 'NO'
        )
    );

    if (!requireAvailable) {

        log('GATE2_CHILD_PROCESS_AVAILABLE=NO');
        log('GATE2_ARDUINO_CLI_EXECUTED=NO');
        log('GATE2_RESULT=LOCAL_PROCESS_API_UNAVAILABLE');

        return code;
    }

    let childProcess = null;

    try {

        childProcess = require('child_process');

    }
    catch (error) {

        log(
            'GATE2_REQUIRE_ERROR=' +
            String(
                error && error.message
                    ? error.message
                    : error
            )
        );

        log('GATE2_CHILD_PROCESS_AVAILABLE=NO');
        log('GATE2_ARDUINO_CLI_EXECUTED=NO');
        log('GATE2_RESULT=CHILD_PROCESS_UNAVAILABLE');

        return code;
    }

    const childProcessAvailable =
        !!childProcess &&
        typeof childProcess.execFile === 'function';

    log(
        'GATE2_CHILD_PROCESS_AVAILABLE=' +
        (
            childProcessAvailable
                ? 'YES'
                : 'NO'
        )
    );

    if (!childProcessAvailable) {

        log('GATE2_ARDUINO_CLI_EXECUTED=NO');
        log('GATE2_RESULT=CHILD_PROCESS_UNAVAILABLE');

        return code;
    }

    if (typeof progressH === 'function') {
        try {
            progressH(10);
        }
        catch (_) {}
    }

    /*
     * Gate 2 usa intencionalmente el CLI localizado
     * previamente en el host.
     *
     * NO será necesariamente el CLI final para JWPLC.
     */
    const cliPath =
        'C:\\Program Files\\Arduino PLC IDE Tools\\arduino-cli.exe';

    log(
        'GATE2_CLI_PROBE_PATH=' +
        cliPath
    );

    const result = await new Promise((resolve) => {

        childProcess.execFile(
            cliPath,
            ['version'],
            {
                windowsHide: true
            },
            (
                error,
                stdout,
                stderr
            ) => {

                resolve({
                    error: error,
                    stdout: String(stdout || '').trim(),
                    stderr: String(stderr || '').trim()
                });
            }
        );
    });

    if (typeof progressH === 'function') {
        try {
            progressH(80);
        }
        catch (_) {}
    }

    const exitCode =
        result.error &&
        typeof result.error.code !== 'undefined'
            ? result.error.code
            : 0;

    log(
        'GATE2_ARDUINO_CLI_EXECUTED=' +
        (
            result.error
                ? 'NO'
                : 'YES'
        )
    );

    log(
        'GATE2_ARDUINO_CLI_EXIT=' +
        String(exitCode)
    );

    if (result.stdout) {

        log(
            'GATE2_ARDUINO_CLI_STDOUT=' +
            result.stdout.replace(/\r?\n/g, ' | ')
        );
    }

    if (result.stderr) {

        log(
            'GATE2_ARDUINO_CLI_STDERR=' +
            result.stderr.replace(/\r?\n/g, ' | ')
        );
    }

    if (result.error) {

        log(
            'GATE2_ERROR=' +
            String(
                result.error.message ||
                result.error
            )
        );

        log('GATE2_RESULT=FAIL');
    }
    else {

        log('GATE2_RESULT=PASS');
    }

    if (typeof progressH === 'function') {
        try {
            progressH(100);
        }
        catch (_) {}
    }

    return code;
}
