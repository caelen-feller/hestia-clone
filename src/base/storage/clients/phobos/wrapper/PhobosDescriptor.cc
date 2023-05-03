#include "PhobosDescriptor.h"

#if HAS_PHOBOS
PhobosDescriptor::PhobosDescriptor()
{
    allocate_handle();
}

PhobosDescriptor::PhobosDescriptor(const Info& info)
{
    allocate_handle();
    set_up_object_operation(info);
}

PhobosDescriptor::~PhobosDescriptor()
{
    clear_object_id();
    pho_xfer_desc_destroy_cpp(&m_handle);
}

void PhobosDescriptor::allocate_handle()
{
    ::memset(&m_handle, 0, sizeof(m_handle));
    m_handle.xd_attrs = {0};
}

void PhobosDescriptor::clear_object_id()
{
    if (m_handle.xd_objid != nullptr) {
        delete[] m_handle.xd_objid;
        m_handle.xd_objid = nullptr;
    }
}

pho_xfer_desc& PhobosDescriptor::get_handle()
{
    return m_handle;
}

void PhobosDescriptor::set_up_object_operation(const Info& info)
{
    clear_object_id();

    char* unconsted_object_id = new char[info.m_object_id.length() + 1];
    ::strcpy(unconsted_object_id, info.m_object_id.c_str());
    m_handle.xd_objid = unconsted_object_id;

    switch (info.m_op) {
        case Operation::PUT:
            m_handle.xd_op = PHO_XFER_OP_PUT;
            m_handle.xd_params.put.family =
                str2rsc_family(m_layout_type.c_str());
            m_handle.xd_params.put.size = info.m_size;
            m_handle.xd_fd              = info.m_fd;
            break;
        case Operation::GET:
            m_handle.xd_op = PHO_XFER_OP_GET;
            m_handle.xd_fd = info.m_fd;
            break;
        case Operation::GET_MD:
            m_handle.xd_op = PHO_XFER_OP_GETMD;
            break;
        case Operation::DEL:
            m_handle.xd_op = PHO_XFER_OP_DEL;
            break;
        default:
            break;
    }
}

#endif