add_library(auditd STATIC
            audit.cc audit.h
            auditconfig.cc auditconfig.h
            audit_interface.cc
            auditfile.cc auditfile.h
            configureevent.cc configureevent.h
            event.cc event.h
            eventdescriptor.cc
            eventdescriptor.h)
target_link_libraries(auditd
                      memcached_logger
                      mcd_time
                      mcd_util
                      platform
                      dirutils)
add_dependencies(auditd generate_audit_descriptors)


