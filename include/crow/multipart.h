#pragma once

#include <string>
#include <vector>
#include <sstream>

#include "crow/http_request.h"
#include "crow/returnable.h"

namespace crow
{
    /// Encapsulates anything related to processing and organizing `multipart/xyz` messages
    namespace multipart
    {
        const std::string dd = "--";

        /// The first part in a section, contains metadata about the part
        struct header
        {
            std::pair<std::string, std::string> value;           ///< The first part of the header, usually `Content-Type` or `Content-Disposition`
            std::unordered_map<std::string, std::string> params; ///< The parameters of the header, come after the `value`
        };

        ///One part of the multipart message

        ///
        /// It is usually separated from other sections by a `boundary`
        struct part
        {
            //TODO(EDev): restructure this to an `unordered_map<string, header>` with string being `header::value.first`
            std::vector<header> headers; ///< (optional) The first part before the data, Contains information regarding the type of data and encoding
            std::string body;            ///< The actual data in the part
        };

        /// The parsed multipart request/response
        struct message : public returnable
        {
            ci_map headers;          ///< The request/response headers
            std::string boundary;    ///< The text boundary that separates different `parts`
            std::vector<part> parts; ///< The individual parts of the message

            const std::string& get_header_value(const std::string& key) const
            {
                return crow::get_header_value(headers, key);
            }

            /// Represent all parts as a string (**does not include message headers**)
            std::string dump() const override
            {
                std::stringstream str;
                std::string delimiter = dd + boundary;

                for (unsigned i = 0; i < parts.size(); i++)
                {
                    str << delimiter << crlf;
                    str << dump(i);
                }
                str << delimiter << dd << crlf;
                return str.str();
            }

            /// Represent an individual part as a string
            std::string dump(int part_) const
            {
                std::stringstream str;
                part item = parts[part_];
                for (header item_h : item.headers)
                {
                    str << item_h.value.first << ": " << item_h.value.second;
                    for (auto& it : item_h.params)
                    {
                        str << "; " << it.first << '=' << pad(it.second);
                    }
                    str << crlf;
                }
                str << crlf;
                str << item.body << crlf;
                return str.str();
            }

            /// Default constructor using default values
            message(const ci_map& headers, const std::string& boundary, const std::vector<part>& sections):
              returnable("multipart/form-data"), headers(headers), boundary(boundary), parts(sections) {}

            /// Create a multipart message from a request data
            message(const request& req):
              returnable("multipart/form-data"),
              headers(req.headers),
              boundary(get_boundary(get_header_value("Content-Type"))),
              parts(parse_body(req.body))
            {}

        private:
            std::string get_boundary(const std::string& header) const
            {
                constexpr char boundary_text[] = "boundary=";
                size_t found = header.find(boundary_text);
                if (found)
                {
                    std::string to_return(header.substr(found + strlen(boundary_text)));
                    if (to_return[0] == '\"')
                    {
                        to_return = to_return.substr(1, to_return.length() - 2);
                    }
                    return to_return;
                }
                return std::string();
            }

            std::vector<part> parse_body(std::string body)
            {

                std::vector<part> sections;

                std::string delimiter = dd + boundary;

                while (body != (crlf))
                {
                    size_t found = body.find(delimiter);
                    std::string section = body.substr(0, found);

                    // +2 is the CRLF.
                    // We don't check it and delete it so that the same delimiter can be used for The last delimiter (--delimiter--CRLF).
                    body.erase(0, found + delimiter.length() + 2);
                    if (!section.empty())
                    {
                        sections.emplace_back(parse_section(section));
                    }
                }
                return sections;
            }

            part parse_section(std::string& section)
            {
                struct part to_return;

                size_t found = section.find(crlf + crlf);
                std::string head_line = section.substr(0, found + 2);
                section.erase(0, found + 4);

                parse_section_head(head_line, to_return);
                to_return.body = section.substr(0, section.length() - 2);
                return to_return;
            }

            void parse_section_head(std::string& lines, part& part)
            {
                while (!lines.empty())
                {
                    header to_add;

                    size_t found = lines.find(crlf);
                    std::string line = lines.substr(0, found);
                    lines.erase(0, found + 2);
                    // Add the header if available
                    if (!line.empty())
                    {
                        size_t found = line.find("; ");
                        std::string header = line.substr(0, found);
                        if (found != std::string::npos)
                            line.erase(0, found + 2);
                        else
                            line = std::string();

                        size_t header_split = header.find(": ");

                        to_add.value = std::pair<std::string, std::string>(header.substr(0, header_split), header.substr(header_split + 2));
                    }

                    // Add the parameters
                    while (!line.empty())
                    {
                        size_t found = line.find("; ");
                        std::string param = line.substr(0, found);
                        if (found != std::string::npos)
                            line.erase(0, found + 2);
                        else
                            line = std::string();

                        size_t param_split = param.find('=');

                        std::string value = param.substr(param_split + 1);

                        to_add.params.emplace(param.substr(0, param_split), trim(value));
                    }
                    part.headers.emplace_back(to_add);
                }
            }

            inline std::string trim(std::string& string, const char& excess = '"') const
            {
                if (string.length() > 1 && string[0] == excess && string[string.length() - 1] == excess)
                    return string.substr(1, string.length() - 2);
                return string;
            }

            inline std::string pad(std::string& string, const char& padding = '"') const
            {
                return (padding + string + padding);
            }
        };
    } // namespace multipart
} // namespace crow
