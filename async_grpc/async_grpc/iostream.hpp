#pragma once

#include <iostream>
#include <grpcpp/grpcpp.h>

const char* to_string(grpc::StatusCode code);
std::ostream& operator<<(std::ostream& out, grpc::StatusCode code);
std::ostream& operator<<(std::ostream& out, const grpc::Status& status);
