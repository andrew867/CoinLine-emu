// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io_shared.h"

int main()
{
	if (coinline_env_value_is_truthy(nullptr) || coinline_env_value_is_truthy(""))
		return 1;
	if (!coinline_env_value_is_truthy("1") || !coinline_env_value_is_truthy("true") || !coinline_env_value_is_truthy("YES")
		|| !coinline_env_value_is_truthy("On"))
		return 2;
	if (coinline_env_value_is_truthy("0") || coinline_env_value_is_truthy("false") || coinline_env_value_is_truthy("no"))
		return 3;
	if (!coinline_env_value_is_falsey("0") || !coinline_env_value_is_falsey("OFF") || coinline_env_value_is_falsey("1")
		|| coinline_env_value_is_falsey(nullptr))
		return 4;
	if (!coinline_env_default_true_unless_falsey(nullptr) || !coinline_env_default_true_unless_falsey("")
		|| !coinline_env_default_true_unless_falsey("maybe"))
		return 5;
	if (coinline_env_default_true_unless_falsey("0") || coinline_env_default_true_unless_falsey("false"))
		return 6;
	return 0;
}
