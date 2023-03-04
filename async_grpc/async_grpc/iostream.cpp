#include "iostream.hpp"
#include <cassert>

const char* to_string(grpc::StatusCode code)
{
  switch (code) {
  case grpc::StatusCode::OK: return "OK";
  case grpc::StatusCode::CANCELLED: return "CANCELLED";
  case grpc::StatusCode::UNKNOWN: return "UNKNOWN";
  case grpc::StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
  case grpc::StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
  case grpc::StatusCode::NOT_FOUND: return "NOT_FOUND";
  case grpc::StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
  case grpc::StatusCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
  case grpc::StatusCode::UNAUTHENTICATED: return "UNAUTHENTICATED";
  case grpc::StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
  case grpc::StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
  case grpc::StatusCode::ABORTED: return "ABORTED";
  case grpc::StatusCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
  case grpc::StatusCode::UNIMPLEMENTED: return "UNIMPLEMENTED";
  case grpc::StatusCode::INTERNAL: return "INTERNAL";
  case grpc::StatusCode::UNAVAILABLE: return "UNAVAILABLE";
  case grpc::StatusCode::DATA_LOSS: return "DATA_LOSS";
  case grpc::StatusCode::DO_NOT_USE: return "DO_NOT_USE";
  default:
    assert(false);
    return "UNKNOWN";
  }
}

std::ostream& operator<<(std::ostream& out, grpc::StatusCode code)
{
  return out << to_string(code) << '(' << static_cast<int>(code) << ')';
}

std::ostream& operator<<(std::ostream& out, const grpc::Status& status)
{
  out << status.error_code();
  if (!status.ok()) {
   out << ": " << status.error_message();
  }
  return out;
}
