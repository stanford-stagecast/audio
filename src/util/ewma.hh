#pragma once

template<typename T1, typename T2>
void ewma_update( T1& variable, const T2& new_value, const float ALPHA )
{
  variable = ALPHA * new_value + ( 1 - ALPHA ) * variable;
}
