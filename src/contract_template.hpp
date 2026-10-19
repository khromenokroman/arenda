#pragma once
#include <string_view>

/// Шаблон договора из templates/contract.html, вшитый при сборке (CMake).
[[nodiscard]] std::string_view contract_template();
