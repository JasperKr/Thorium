---@meta Math

error("Do not require this file")

--[[
{"eulerToQuaternion", wrap_EulerToQuaternion},
    {"eulerToMatrix", wrap_EulerToMatrix},
    {"quaternionToEuler", wrap_QuaternionToEuler},
    {"quaternionToMatrix", wrap_QuaternionToMatrix},
    {"matrixToEuler", wrap_MatrixToEuler},
    {"matrixToQuaternion", wrap_MatrixToQuaternion},
    {"random", wrap_Random},
    {"noise", wrap_Noise},
    {"noiseWrapped", wrap_NoiseWrapped},
]]

--- Converts an euler angle to a quaternion
--- @param yaw number
--- @param pitch number
--- @param roll number
--- @return number x
--- @return number y
--- @return number z
--- @return number w
function Thorium.math.eulerToQuaternion(yaw, pitch, roll) end

--- Converts an euler angle to a matrix4x4
--- @param yaw number
--- @param pitch number
--- @param roll number
--- @return ... matrix
function Thorium.math.eulerToMatrix(yaw, pitch, roll) end

--- Converts a quaternion to an euler angle
--- @param x number
--- @param y number
--- @param z number
--- @param w number
--- @return number yaw
--- @return number pitch
--- @return number roll
function Thorium.math.quaternionToEuler(x, y, z, w) end

--- Converts a quaternion to a matrix4x4
--- @param x number
--- @param y number
--- @param z number
--- @param w number
--- @return number[16] matrix
function Thorium.math.quaternionToMatrix(x, y, z, w) end

--- Converts a matrix4x4 to an euler angle
--- @param matrix number[16]
--- @return number yaw
--- @return number pitch
--- @return number roll
function Thorium.math.matrixToEuler(matrix) end

--- Converts a matrix4x4 to a quaternion
--- @param matrix number[16]
--- @return number x
--- @return number y
--- @return number z
--- @return number w
function Thorium.math.matrixToQuaternion(matrix) end

--- Returns a random integer between min and max
--- @overload fun(max: integer): integer Returns a random integer between 0 and max inclusive
--- @overload fun(): number Returns a random float between 0 and 1
--- @param min integer
--- @param max integer
--- @return integer random
function Thorium.math.random(min, max) end

--- Returns a noise value between -1 and 1 for the given channels
--- @overload fun(x: number, y: number): number Returns a noise value for 2D coordinates
--- @overload fun(x: number): number Returns a noise value for 1D coordinates
--- @param x number
--- @param y number
--- @param z number
--- @return number noise
function Thorium.math.noise(x, y, z) end

--- Returns a noise value between -1 and 1 for the given channels, wrapping around at the given period
--- @overload fun(x: number, y: number, xWrapping: number, yWrapping: number): number Returns a noise value for 2D coordinates
--- @overload fun(x: number, xWrapping: number): number Returns a noise value for 1D coordinates
--- @param x number
--- @param y number
--- @param z number
--- @param xWrapping number
--- @param yWrapping number
--- @param zWrapping number
--- @return number noise
function Thorium.math.noiseWrapped(x, y, z, xWrapping, yWrapping, zWrapping) end
