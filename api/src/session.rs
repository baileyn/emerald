pub struct Session {
    ip: Option<u32>,
    account_id: u32,
    session_id: String,
}

impl Session {
    pub fn new(account_id: u32, session_id: String) -> Self {
        Self {
            ip: None,
            account_id,
            session_id,
        }
    }

    pub fn set_ip(&mut self, ip: u32) {
        self.ip = Some(ip)
    }

    pub fn account_id(&self) -> u32 {
        self.account_id
    }
}