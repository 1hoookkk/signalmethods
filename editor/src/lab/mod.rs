use crate::services::Services;
use crate::session::state::Session;

pub fn session_for(_services: &mut Services, _fixture: &str) -> Result<Session, String> {
    Ok(Session::new())
}
