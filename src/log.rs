use std::cell::Cell;
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

thread_local! {
    static LOG_ENABLED: Cell<bool> = const { Cell::new(false) };
}

static LOG_FILE: Mutex<Option<File>> = Mutex::new(None);

#[derive(Clone, Copy)]
pub enum LogType {
    Verbose,
    Info,
    Warning,
    Error,
}

impl LogType {
    fn as_str(&self) -> &'static str {
        match self {
            LogType::Verbose => "VERBOSE",
            LogType::Info => "INFO",
            LogType::Warning => "WARNING",
            LogType::Error => "ERROR",
        }
    }
}

pub fn set_logs(enabled: bool) {
    LOG_ENABLED.with(|l| l.set(enabled));
}

pub fn log_printf(log_type: LogType, msg: &str) {
    if !LOG_ENABLED.with(|l| l.get()) {
        return;
    }
    let mut guard = LOG_FILE.lock().unwrap();
    if guard.is_none() {
        *guard = OpenOptions::new()
            .create(true)
            .append(true)
            .open("gut_archive.log")
            .ok();
    }
    if let Some(ref mut file) = *guard {
        let _ = writeln!(file, "[{}] {}", log_type.as_str(), msg);
        let _ = file.flush();
    }
}

pub fn close_log() {
    let mut guard = LOG_FILE.lock().unwrap();
    *guard = None;
}
