use std::path::Path;
use std::fs;
use std::io;

use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct GlobalConfig {
    database: DatabaseConfig,
}

#[derive(Debug, Deserialize)]
pub struct DatabaseConfig {
    host: String,
    port: u16,
    database: String,
    username: String,
    password: Option<String>,
}

pub fn load_global<P: AsRef<Path>>(path: P) -> io::Result<GlobalConfig> {
    let data = fs::read_to_string(path)?;
    Ok(toml::from_str(&data)?)
}
