const express = require('express');
const cors = require('cors');
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

const app = express();
const port = 3001;

app.use(cors());
app.use(express.json());

// Paths
const REPO_ROOT = path.join(__dirname, '..');
const INJECTOR_PATH = path.join(REPO_ROOT, 'build', 'bloodstrike-injector');
const LIB_PATH = path.join(REPO_ROOT, 'build', 'libbloodstrike.so');

// 🔍 GET /status: Check if process is running
app.get('/status', (req, res) => {
    // Check for BloodStrike.exe (Proton/Wine commonly) or bloodstrike (native/alias)
    exec('pgrep -f "BloodStrike.exe"', (error, stdout, stderr) => {
        const isRunning = !!stdout.trim();

        // Check Ptrace Scope
        exec('cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0', (pError, pStdout) => {
            const ptraceScope = pStdout.trim();

            // Fix: Correctly query SELinux status rather than hardcoding
            exec('getenforce 2>/dev/null || echo "Disabled"', (sError, sStdout) => {
                const seLinux = sStdout.trim() || 'Disabled';

                res.json({
                    gameRunning: isRunning,
                    ptraceScope: ptraceScope === '0' ? 'OK' : 'RESTRICTED',
                    seLinux: seLinux.toUpperCase(),
                    pid: isRunning ? stdout.trim().split('\n')[0] : null
                });
            });
        });
    });
});

// ⚡ POST /inject: Trigger the C++ injector via pkexec (GUI password prompt)
app.post('/inject', (req, res) => {
    const { method, pid } = req.body;
    const target = pid || 'bloodstrike';
    const injectionMethod = method === 'proton' ? '4' : (method === 'memfd' ? '2' : '1');

    // Quote all arguments to handle paths with spaces
    // Added -v for verbose output to help catch the SIGSEGV logs you saw
    const command = `pkexec "${INJECTOR_PATH}" -p "${target}" -l "${LIB_PATH}" -m ${injectionMethod} -v`;

    console.log(`[bridge] Executing: ${command}`);

    // 30-second timeout to prevent the bridge hanging on a stalled injector
    const injectorProcess = exec(command, { timeout: 30000 });
    let output = '';

    injectorProcess.stdout.on('data', (data) => {
        output += data.toString();
        console.log(data.toString());
    });

    injectorProcess.stderr.on('data', (data) => {
        output += data.toString();
    });

    injectorProcess.on('close', (code, signal) => {
        // code is null when killed by signal/timeout
        const timedOut = signal === 'SIGTERM' && !output.includes('signal=11');
        const crashed = signal === 'SIGSEGV' || output.includes('signal=11');

        let statusMessage = '';
        if (timedOut) statusMessage = '\n[bridge] Injector timed out after 30s';
        if (crashed) statusMessage = '\n[bridge] Critical: Injector or Shellcode crashed (SIGSEGV). Check memory offsets.';

        const success = code === 0 && !crashed;

        res.json({
            success: success,
            output: output + statusMessage,
            code: code || signal
        });
    });
});

app.listen(port, () => {
    console.log(`[bridge] System bridge listening at http://localhost:${port}`);
});
