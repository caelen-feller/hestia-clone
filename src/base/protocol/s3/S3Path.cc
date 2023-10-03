#include "S3Path.h"

namespace hestia {

S3Path::S3Path(const HttpRequest& request)
{
    if (const auto& host = request.get_header().get_item("Host");
        !host.empty()) {
        m_container_name     = host;
        const auto full_path = request.get_path();
        m_object_id          = full_path.substr(1, full_path.size());
    }
    else {
        from_path_only(request.get_path());
    }
}

S3Path::S3Path(const std::string& path)
{
    from_path_only(path);
}

enum class LineState {
    AWAITING_FIRST_NON_SLASH,
    AWAITING_CONTAINER_END,
    AWAITING_OBJECT_END,
    AWAITING_QUERY_END
};

void S3Path::from_path_only(const std::string& path)
{
    if (path.empty() || path == "/") {
        return;
    }

    auto line_state = LineState::AWAITING_FIRST_NON_SLASH;
    for (const auto c : path) {
        if (c == '/') {
            if (line_state == LineState::AWAITING_FIRST_NON_SLASH) {
                continue;
            }
            else if (line_state == LineState::AWAITING_CONTAINER_END) {
                line_state = LineState::AWAITING_OBJECT_END;
            }
        }
        else {
            if (line_state == LineState::AWAITING_FIRST_NON_SLASH) {
                line_state = LineState::AWAITING_CONTAINER_END;
                m_container_name += c;
            }
            else if (line_state == LineState::AWAITING_CONTAINER_END) {
                if (c == '?') {
                    line_state = LineState::AWAITING_QUERY_END;
                }
                else {
                    m_container_name += c;
                }
            }
            else if (line_state == LineState::AWAITING_OBJECT_END) {
                if (m_object_id.empty() && c == '?') {
                    line_state = LineState::AWAITING_QUERY_END;
                }
                else {
                    m_object_id += c;
                }
            }
            else if (line_state == LineState::AWAITING_QUERY_END) {
                m_queries += c;
            }
        }
    }
}

}  // namespace hestia