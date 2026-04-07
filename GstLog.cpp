#include "GstLog.h"
#include "GstLogger.h"

namespace GST {
namespace LOG {

std::shared_ptr<GST::LOG::GstLogger> get_Instance() {
    return GstLogger::get_Instance();
}

} // namespace LOG
} // namespace GST
