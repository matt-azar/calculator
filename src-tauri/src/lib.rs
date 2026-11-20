mod pty;

use std::sync::Mutex;

use pty::PtySession;
use tauri::{AppHandle, Manager, State};

struct AppState {
    session: Mutex<Option<PtySession>>,
}

#[tauri::command]
fn pty_start(app: AppHandle, state: State<'_, AppState>, cols: u16, rows: u16) -> Result<(), String> {
    let mut guard = state
        .session
        .lock()
        .map_err(|_| "PTY state lock poisoned".to_string())?;
    if guard.is_some() {
        return Ok(());
    }
    let session = PtySession::spawn(&app, cols, rows)?;
    *guard = Some(session);
    Ok(())
}

#[tauri::command]
fn pty_write(state: State<'_, AppState>, data: String) -> Result<(), String> {
    let bytes = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, data)
        .map_err(|err| format!("Invalid PTY input encoding: {err}"))?;
    let mut guard = state
        .session
        .lock()
        .map_err(|_| "PTY state lock poisoned".to_string())?;
    let session = guard.as_mut().ok_or_else(|| "IPython session is not running".to_string())?;
    session.write_all(&bytes)
}

#[tauri::command]
fn pty_resize(state: State<'_, AppState>, cols: u16, rows: u16) -> Result<(), String> {
    let mut guard = state
        .session
        .lock()
        .map_err(|_| "PTY state lock poisoned".to_string())?;
    if let Some(session) = guard.as_mut() {
        session.resize(cols, rows)?;
    }
    Ok(())
}

#[tauri::command]
fn pty_stop(state: State<'_, AppState>) -> Result<(), String> {
    let mut guard = state
        .session
        .lock()
        .map_err(|_| "PTY state lock poisoned".to_string())?;
    if let Some(session) = guard.take() {
        session.kill();
    }
    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(AppState {
            session: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            pty_start,
            pty_write,
            pty_resize,
            pty_stop
        ])
        .setup(|app| {
            // Ensure the resource directory is discoverable in both dev and packaged builds.
            let _ = app.path().resource_dir();
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
