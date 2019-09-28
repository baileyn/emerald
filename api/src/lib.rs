use database;
use database::models::Account;

use rand::Rng;

const SESSION_ID_LENGTH: usize = 28;

struct Api {
    db: diesel::pg::PgConnection,
}

impl Api {
    pub fn new() -> Self {
        Self {
            db: database::establish_connection(),
        }
    }

    pub fn login<S: AsRef<str>>(&self, username: S, password: S) -> Option<String> {
        let account = Account::with_credentials(&self.db, username, password);

        if let Some(account) = account {
            let sid = generate_session_id();
            Some(sid)
        } else {
            None
        }
    }
}

fn generate_session_id() -> String {
    let mut sid = String::new();
    let mut rng = rand::thread_rng();

    for i in 0..SESSION_ID_LENGTH/2 {
        let number = 0x1111 + rng.gen::<u16>() as u32;
        sid.push_str(&format!("{:04X}", number as u16));
    }

    sid
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_session_id_generation() {
        let sid = generate_session_id();

        assert_eq!(sid.len() / 2, SESSION_ID_LENGTH);
    }
}