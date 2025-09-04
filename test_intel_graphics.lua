-- Test configuration for Intel graphics simulation
-- This file allows testing Intel graphics workarounds without Intel hardware

-- To enable Intel graphics simulation, set this to true
g_cef_test_intel_graphics = true

-- Optional: You can also test different scenarios
g_cef_allow_intel_override = false  -- Test the override behavior

-- Instructions:
-- 1. Copy this file to your OTClient directory
-- 2. Load it in your init.lua or main configuration
-- 3. The CEF system will behave as if you have Intel graphics
-- 4. Check the logs for Intel-specific messages:
--    - "TEST MODE: Simulating Intel graphics behavior"
--    - "Intel graphics detected - applying compatibility workarounds"
--    - "Intel compatibility flags applied"

print("Intel graphics test mode configuration loaded")
print("Intel simulation: " .. tostring(g_cef_test_intel_graphics))