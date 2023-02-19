/*
 *
 * Copyright 2019 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_INTERNAL_COMPILER_CPP_PLUGIN_H
#define GRPC_INTERNAL_COMPILER_CPP_PLUGIN_H

#include <memory>
#include <sstream>

#include "src/compiler/config.h"
#include "src/compiler/cpp_generator.h"
#include "src/compiler/generator_helpers.h"
#include "src/compiler/protobuf_plugin.h"

// Cpp Generator for Protobug IDL
class CppGrpcGenerator : public grpc::protobuf::compiler::CodeGenerator {
 public:
  CppGrpcGenerator() {}
  virtual ~CppGrpcGenerator() {}

  uint64_t GetSupportedFeatures() const override {
    return FEATURE_PROTO3_OPTIONAL;
  }

  virtual bool GenerateAll(const std::vector<const FileDescriptor*>& files,
                           const std::string& parameter,
                           GeneratorContext* context,
                           std::string* error) const override {
    grpc_cpp_generator::Parameters generator_parameters;
    generator_parameters.use_system_headers = true;
    generator_parameters.generate_mock_code = false;
    generator_parameters.include_import_headers = false;

    std::string code;
#ifdef _DEBUG
    WriteSummaryFile(files, context);
#endif  // _DEBUG

    // services.h
    code.clear();
    auto first_file = ProtoBufFile(files.front());
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> services_header_output(
        context->Open("services.h"));
    code += grpc_cpp_generator::GetServicesHeaderPrologue(
      &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesHeaderForwardDeclarations(
          &pbfile, generator_parameters);
    }
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesHeaderPointerDeclarations(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetServicesHeaderClassPrologue(
        &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesHeaderClassDeclaration(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetServicesHeaderClassEpilogue(
      &first_file, generator_parameters);
    code += grpc_cpp_generator::GetServicesHeaderEpilogue(
      &first_file, generator_parameters);

    grpc::protobuf::io::CodedOutputStream coded_services_out(services_header_output.get());
    coded_services_out.WriteRaw(code.data(), code.size());

    // services.cc
    code.clear();
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> services_source_output(
        context->Open("services.cc"));
    code += grpc_cpp_generator::GetServicesSourcePrologue(
      &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesSourceIncludes(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetServicesSourceConstructorPrologue(
        &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesSourceConstructorDeclaration(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetServicesSourceConstructorEpilogue(
        &first_file, generator_parameters);
    code += grpc_cpp_generator::GetServicesSourceDestructor(
        &first_file, generator_parameters);
    code += grpc_cpp_generator::GetServicesSourceMethodPrologue(
        &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetServicesSourceMethodCall(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetServicesSourceMethodEpilogue(
        &first_file, generator_parameters);
    grpc::protobuf::io::CodedOutputStream coded_services_source_out(
        services_source_output.get());
    coded_services_source_out.WriteRaw(code.data(), code.size());

    // summary.h
    code.clear();
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> summary_header_output(
      context->Open("packages.xml"));
    code += grpc_cpp_generator::GetPackagesXmlPrologue(
      &first_file, generator_parameters);
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetPackagesXmlIncludes(
          &pbfile, generator_parameters);
    }
    for (const auto& file : files) {
      ProtoBufFile pbfile(file);
      code += grpc_cpp_generator::GetPackagesXmlMethods(
          &pbfile, generator_parameters);
    }
    code += grpc_cpp_generator::GetPackagesXmlEpilogue(
        &first_file, generator_parameters);
    grpc::protobuf::io::CodedOutputStream coded_summary_header_out(
        summary_header_output.get());
    coded_summary_header_out.WriteRaw(code.data(), code.size());

    return grpc::protobuf::compiler::CodeGenerator::GenerateAll(
        files, parameter, context, error);
  }

  virtual bool Generate(const grpc::protobuf::FileDescriptor* file,
                        const std::string& parameter,
                        grpc::protobuf::compiler::GeneratorContext* context,
                        std::string* error) const override {
    if (file->options().cc_generic_services()) {
      *error =
          "cpp grpc proto compiler plugin does not work with generic "
          "services. To generate cpp grpc APIs, please set \""
          "cc_generic_service = false\".";
      return false;
    }

    grpc_cpp_generator::Parameters generator_parameters;
    generator_parameters.use_system_headers = true;
    generator_parameters.generate_mock_code = false;
    generator_parameters.include_import_headers = false;

    ProtoBufFile pbfile(file);

    if (!parameter.empty()) {
      std::vector<std::string> parameters_list =
          grpc_generator::tokenize(parameter, ",");
      for (auto parameter_string = parameters_list.begin();
           parameter_string != parameters_list.end(); parameter_string++) {
        std::vector<std::string> param =
            grpc_generator::tokenize(*parameter_string, "=");
        if (param[0] == "services_namespace") {
          generator_parameters.services_namespace = param[1];
        } else if (param[0] == "use_system_headers") {
          if (param[1] == "true") {
            generator_parameters.use_system_headers = true;
          } else if (param[1] == "false") {
            generator_parameters.use_system_headers = false;
          } else {
            *error = std::string("Invalid parameter: ") + *parameter_string;
            return false;
          }
        } else if (param[0] == "grpc_search_path") {
          generator_parameters.grpc_search_path = param[1];
        } else if (param[0] == "generate_mock_code") {
          if (param[1] == "true") {
            generator_parameters.generate_mock_code = true;
          } else if (param[1] != "false") {
            *error = std::string("Invalid parameter: ") + *parameter_string;
            return false;
          }
        } else if (param[0] == "gmock_search_path") {
          generator_parameters.gmock_search_path = param[1];
        } else if (param[0] == "additional_header_includes") {
          generator_parameters.additional_header_includes =
              grpc_generator::tokenize(param[1], ":");
        } else if (param[0] == "message_header_extension") {
          generator_parameters.message_header_extension = param[1];
        } else if (param[0] == "include_import_headers") {
          if (param[1] == "true") {
            generator_parameters.include_import_headers = true;
          } else if (param[1] != "false") {
            *error = std::string("Invalid parameter: ") + *parameter_string;
            return false;
          }
        } else {
          *error = std::string("Unknown parameter: ") + *parameter_string;
          return false;
        }
      }
    }

    std::string file_name = grpc_generator::StripProto(file->name());

    // Stub header
    std::string stub_header_code = 
      grpc_cpp_generator::GetStubHeaderPrologue(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubHeaderIncludes(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubHeaderServices(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubHeaderEpilogue(&pbfile, generator_parameters);
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> stub_header_output(
        context->Open(file_name + ".stub.h"));
    grpc::protobuf::io::CodedOutputStream stub_header_coded_out(stub_header_output.get());
    stub_header_coded_out.WriteRaw(stub_header_code.data(), stub_header_code.size());

    // Stub source
    std::string stub_source_code = 
      grpc_cpp_generator::GetStubSourcePrologue(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubSourceIncludes(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubSourceServices(&pbfile, generator_parameters) +
      grpc_cpp_generator::GetStubSourceEpilogue(&pbfile, generator_parameters);
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> stub_source_output(
      context->Open(file_name + ".stub.cc"));
    grpc::protobuf::io::CodedOutputStream stub_source_coded_out(
        stub_source_output.get());
    stub_source_coded_out.WriteRaw(stub_source_code.data(), stub_source_code.size());

    std::string header_code =
        grpc_cpp_generator::GetHeaderPrologue(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetHeaderIncludes(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetHeaderServices(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetHeaderEpilogue(&pbfile, generator_parameters);
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> header_output(
        context->Open(file_name + ".grpc.pb.h"));
    grpc::protobuf::io::CodedOutputStream header_coded_out(header_output.get());
    header_coded_out.WriteRaw(header_code.data(), header_code.size());

    std::string source_code =
        grpc_cpp_generator::GetSourcePrologue(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetSourceIncludes(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetSourceServices(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetSourceEpilogue(&pbfile, generator_parameters);
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> source_output(
        context->Open(file_name + ".grpc.pb.cc"));
    grpc::protobuf::io::CodedOutputStream source_coded_out(source_output.get());
    source_coded_out.WriteRaw(source_code.data(), source_code.size());

    if (!generator_parameters.generate_mock_code) {
      return true;
    }
    std::string mock_code =
        grpc_cpp_generator::GetMockPrologue(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetMockIncludes(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetMockServices(&pbfile, generator_parameters) +
        grpc_cpp_generator::GetMockEpilogue(&pbfile, generator_parameters);
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> mock_output(
        context->Open(file_name + "_mock.grpc.pb.h"));
    grpc::protobuf::io::CodedOutputStream mock_coded_out(mock_output.get());
    mock_coded_out.WriteRaw(mock_code.data(), mock_code.size());

    return true;
  }

 private:
  // Insert the given code into the given file at the given insertion point.
  void Insert(grpc::protobuf::compiler::GeneratorContext* context,
              const std::string& filename, const std::string& insertion_point,
              const std::string& code) const {
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> output(
        context->OpenForInsert(filename, insertion_point));
    grpc::protobuf::io::CodedOutputStream coded_out(output.get());
    coded_out.WriteRaw(code.data(), code.size());
  }

  void WriteSummaryFile(const std::vector<const FileDescriptor*>& files,
                        GeneratorContext* context) const {
    std::string code;

    // Print the quantity of proto-files found
    code += "Proto-files found: ";
    code += std::to_string(files.size());
    code += "\n";

    // Print the total number of packages, services and methods
    int totalFileWithServicesCount = 0;
    int totalServiceCount = 0;
    int totalMethodCount = 0;
    for (auto file : files) {
      if (file->service_count() > 0) {
        totalFileWithServicesCount++;
      }
      totalServiceCount += file->service_count();
      for (int i = 0; i < file->service_count(); i++) {
        totalMethodCount += file->service(i)->method_count();
      }
    }
    code += "Proto-files with services found: ";
    code += std::to_string(totalFileWithServicesCount);
    code += "\n";
    code += "Services found: ";
    code += std::to_string(totalServiceCount);
    code += "\n";
    code += "Methods found: ";
    code += std::to_string(totalMethodCount);
    code += "\n";

    for (auto file : files) {
      code += file->name();
      code += "\n";
    }
    std::unique_ptr<grpc::protobuf::io::ZeroCopyOutputStream> output(
        context->Open("__report__.log"));
    grpc::protobuf::io::CodedOutputStream coded_out(output.get());
    coded_out.WriteRaw(code.data(), code.size());
  }
};

#endif  // GRPC_INTERNAL_COMPILER_CPP_PLUGIN_H
