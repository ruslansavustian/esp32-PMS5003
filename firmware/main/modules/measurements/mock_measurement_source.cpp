#include "modules/measurements/measurement_source.h"
#include "esp_random.h"

namespace measurement_source {
Reading read()
{
    // Synthetic values only; cumulative concentrations stay ordered.
    const float pm1 = (esp_random() % 201) / 10.0F;
    const float pm25 = pm1 + (esp_random() % 101) / 10.0F;
    const float pm10 = pm25 + (esp_random() % 151) / 10.0F;
    return {pm1, pm25, pm10, "mock"};
}
}
