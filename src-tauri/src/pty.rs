use std::{
    io::{Read, Write},
    path::{Path, PathBuf},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    thread,
    time::Duration,
};

use base64::{engine::general_purpose::STANDARD as B64, Engine};
use portable_pty::{native_pty_system, ChildKiller, CommandBuilder, MasterPty, PtySize};
use tauri::{AppHandle, Emitter, Manager};

pub struct PtySession {
    writer: Mutex<Box<dyn Write + Send>>,
    master: Mutex<Box<dyn MasterPty + Send>>,
    killer: Mutex<Box<dyn ChildKiller + Send + Sync>>,
    stop: Arc<AtomicBool>,
}

impl PtySession {
    pub fn spawn(app: &AppHandle, cols: u16, rows: u16) -> Result<Self, String> {
        let preload = resolve_preload(app)?;
        let (program, mut args) = resolve_ipython()?;
        args.extend([
            "-i".to_string(),
            preload.to_string_lossy().into_owned(),
        ]);

        let pty_system = native_pty_system();
        let pair = pty_system
            .openpty(PtySize {
                rows: rows.max(1),
                cols: cols.max(1),
                pixel_width: 0,
                pixel_height: 0,
            })
            .map_err(|err| format!("Could not open PTY: {err}"))?;

        let mut cmd = CommandBuilder::new(&program);
        for arg in &args {
            cmd.arg(arg);
        }
        cmd.env("TERM", "xterm-256color");
        cmd.env("COLORTERM", "truecolor");
        cmd.env("PYTHONUNBUFFERED", "1");
        cmd.env("PYTHONIOENCODING", "utf-8");

        let mut child = pair
            .slave
            .spawn_command(cmd)
            .map_err(|err| format!("Could not start IPython ({program}): {err}"))?;

        // Drop the slave handle in this process so only the child owns it.
        drop(pair.slave);

        let mut reader = pair
            .master
            .try_clone_reader()
            .map_err(|err| format!("Could not read from PTY: {err}"))?;
        let writer = pair
            .master
            .take_writer()
            .map_err(|err| format!("Could not write to PTY: {err}"))?;

        let stop = Arc::new(AtomicBool::new(false));
        let stop_reader = Arc::clone(&stop);
        let stop_waiter = Arc::clone(&stop);
        let app_reader = app.clone();
        let app_waiter = app.clone();

        thread::spawn(move || {
            let mut buf = [0u8; 8192];
            while !stop_reader.load(Ordering::Relaxed) {
                match reader.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        let payload = B64.encode(&buf[..n]);
                        let _ = app_reader.emit("pty-data", payload);
                    }
                    Err(err) if err.kind() == std::io::ErrorKind::Interrupted => continue,
                    Err(_) => break,
                }
            }
        });

        let killer = child.clone_killer();
        thread::spawn(move || {
            loop {
                if stop_waiter.load(Ordering::Relaxed) {
                    break;
                }
                match child.try_wait() {
                    Ok(Some(status)) => {
                        let _ = app_waiter.emit("pty-exit", status.to_string());
                        break;
                    }
                    Ok(None) => thread::sleep(Duration::from_millis(100)),
                    Err(err) => {
                        let _ = app_waiter.emit("pty-error", err.to_string());
                        break;
                    }
                }
            }
        });

        Ok(Self {
            writer: Mutex::new(writer),
            master: Mutex::new(pair.master),
            killer: Mutex::new(killer),
            stop,
        })
    }

    pub fn write_all(&self, bytes: &[u8]) -> Result<(), String> {
        let mut writer = self
            .writer
            .lock()
            .map_err(|_| "PTY writer lock poisoned".to_string())?;
        writer
            .write_all(bytes)
            .and_then(|_| writer.flush())
            .map_err(|err| format!("Could not write to IPython: {err}"))
    }

    pub fn resize(&self, cols: u16, rows: u16) -> Result<(), String> {
        let master = self
            .master
            .lock()
            .map_err(|_| "PTY master lock poisoned".to_string())?;
        master
            .resize(PtySize {
                rows: rows.max(1),
                cols: cols.max(1),
                pixel_width: 0,
                pixel_height: 0,
            })
            .map_err(|err| format!("Could not resize PTY: {err}"))
    }

    pub fn kill(self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Ok(mut killer) = self.killer.lock() {
            let _ = killer.kill();
        }
    }
}

fn resolve_preload(app: &AppHandle) -> Result<PathBuf, String> {
    let candidates = [
        app.path()
            .resource_dir()
            .ok()
            .map(|dir| dir.join("preload.py")),
        app.path()
            .resource_dir()
            .ok()
            .map(|dir| dir.join("python").join("preload.py")),
        std::env::current_dir()
            .ok()
            .map(|dir| dir.join("python").join("preload.py")),
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .map(|root| root.join("python").join("preload.py")),
    ];

    for candidate in candidates.into_iter().flatten() {
        if candidate.is_file() {
            return Ok(candidate);
        }
    }

    Err(
        "Could not find python/preload.py. Run from the repo root or install the bundled resources."
            .into(),
    )
}

fn resolve_ipython() -> Result<(String, Vec<String>), String> {
    if let Ok(path) = which::which("ipython") {
        return Ok((path_to_string(&path)?, Vec::new()));
    }
    if let Ok(path) = which::which("ipython3") {
        return Ok((path_to_string(&path)?, Vec::new()));
    }

    for python in ["python", "python3", "py"] {
        if let Ok(path) = which::which(python) {
            return Ok((
                path_to_string(&path)?,
                vec!["-m".into(), "IPython".into()],
            ));
        }
    }

    Err(
        "IPython was not found. Install it with `pip install ipython` and ensure it is on PATH."
            .into(),
    )
}

fn path_to_string(path: &Path) -> Result<String, String> {
    path.to_str()
        .map(str::to_owned)
        .ok_or_else(|| format!("Non-UTF8 path: {}", path.display()))
}
