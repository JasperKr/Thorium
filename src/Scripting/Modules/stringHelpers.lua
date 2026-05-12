local stringh = {}

--- Returns the filename from a path
--- Example: C:/Users/JohnDoe/Desktop/MyFile.txt -> MyFile.txt
--- @param path string
--- @return string
function stringh.filename(path)
  return path:match("^.+[\\/]([^/]+)$") or path
end

--- Returns the extension from a filename
--- Example: MyFile.txt -> .txt
--- @param filename string
--- @return string
function stringh.extension(filename)
  return filename:match("^.+(%..+)$")
end

--- Returns the filename without the extension
--- Example: MyFile.txt -> MyFile
--- @param filename string
--- @return string
function stringh.filenameWithoutExtension(filename)
  return filename:match("^(.-)%.") or filename
end

--- Returns the directory from a path
--- Example: C:/Users/JohnDoe/Desktop/MyFile.txt -> C:/Users/JohnDoe/Desktop
--- @param path string
--- @return string
function stringh.directory(path)
  return path:match("^(.+)/[^/]+$")
end

--- Sanitises the filepath
--- Example: C:\Users\JohnDoe\Desktop/MyFile.txt -> C:/Users/JohnDoe/Desktop/MyFile.txt
--- @param path string
--- @return string
function stringh.sanitise(path)
  return path:gsub("\\", "/") or error("Invalid path")
end

--- Checks if a file is of a certain extension
--- Example: C:/Users/JohnDoe/Desktop/MyFile.txt, txt -> true
--- @param path string
--- @param ext string
--- @return boolean
function stringh.hasExtension(path, ext)
  return path:match("^.+%.(" .. ext .. ")$")
end

--- Splits a string into a table
--- Example: "Hello, world, how are you?" -> {"Hello", "world", "how", "are", "you?"}
--- Default separator is " "
--- @param str string
--- @param sep string
--- @param out table?
--- @return table
function stringh.split(str, sep, out)
  out = out or {}
  sep = sep or " "
  for s in str:gmatch("([^" .. sep .. "]+)") do
    table.insert(out, s)
  end
  return out
end

local tempTable = {}

--- Combines paths
--- Example: C:/Users/JohnDoe/Desktop, MyFile.txt -> C:/Users/JohnDoe/Desktop/MyFile.txt
--- @param ... string
--- @return string
function stringh.combinePath(...)
  table.clear(tempTable)
  local count = select("#", ...)

  for i = 1, count do
    local path = stringh.sanitise(select(i, ...))

    if not path or path == "" then
      goto continue
    end

    -- remove "./" but not "../"
    path = path:gsub("^%./", ""):gsub("/%./", "/")

    -- remove trailing and leading slashes
    if path:sub(-1) == "/" then path = path:sub(1, -2) end
    if path:sub(1, 1) == "/" then path = path:sub(2) end

    table.insert(tempTable, path)
    ::continue::
  end

  return table.concat(tempTable, "/")
end

local function toTableVal(x)
  if type(x) == "string" then
    return "\"" .. x .. "\""
  else
    return tostring(x)
  end
end

local function add(t, ...)
  for i = 1, select("#", ...) do
    local val = select(i, ...)
    table.insert(t, val)
  end
end

local function tableToStringInternal(t, strData, stack, searched)
  table.insert(strData, ("  "):rep(#stack))
  if stack[#stack] then
    add(strData, "[", toTableVal(stack[#stack]), "] = ", "{\n")
  else
    table.insert(strData, "{\n")
  end

  for key, v in pairs(t) do
    if type(v) == "table" then
      if searched[v] then
        add(strData,
          ("  "):rep(#stack + 1),
          "[", toTableVal(key), "] = \"Reference to ", searched[v], "\"\n")
      elseif v == t or v == _G then
        add(strData,
          ("  "):rep(#stack + 1), "[", toTableVal(key), "] = \"Reference to self\"\n")
      elseif not next(v) then
        add(strData, ("  "):rep(#stack + 1), "[", toTableVal(key), "] = {}\n")
      elseif t.__tostring then
        add(strData, ("  "):rep(#stack + 1), tostring(v), "\n")
      else
        add(stack, key)
        searched[v] = table.concat(stack, ".")
        tableToStringInternal(v, strData, stack, searched)
        table.remove(stack, #stack)
      end
    else
      if type(v) == "userdata" or type(v) == "function" or type(v) == "thread" then
        add(strData, ("  "):rep(#stack + 1), key, " = \"", type(v), "\"\n")
      elseif type(key) == "string" then
        add(strData, ("  "):rep(#stack + 1), key, " = ", toTableVal(v), "\n")
      else
        add(strData, ("  "):rep(#stack + 1), "[", toTableVal(key), "] = ", toTableVal(v), "\n")
      end
    end
  end
  add(strData, ("  "):rep(#stack), "}\n")
end

--- Converts any object to a string.
--- @param t any
--- @return string
function SnapEngine.internal.tableToString(t)
  local names = {}
  if type(t) == "table" then
    if t.__tostring then
      return tostring(t)
    end
    local data = {}
    tableToStringInternal(t, data, names, {})
    return table.concat(data)
  else
    return tostring(t)
  end
end

--- Prints a table to the console.
--- @param t table
function SnapEngine.internal.printTable(t)
  local str = SnapEngine.internal.tableToString(t)
  print(str)
end

--- Prints a table to the console, but only one level deep.
---@param t table
---@param floor boolean If true, will round numbers to the nearest integer.
function SnapEngine.internal.shallowPrintTable(t, floor)
  if type(t) == "table" then
    io.write("{\n")
    for i, v in pairs(t) do
      if type(v) == "table" then
        io.write("\t" .. tostring(i) .. " = {}\n")
      else
        if floor then
          if type(v) == "number" then
            io.write("\t" .. tostring(i) .. " = " .. SnapEngine.math.round(v) .. "\n")
          else
            io.write("\t" .. tostring(i) .. " = " .. v .. "\n")
          end
        else
          io.write("\t" .. tostring(i) .. " = " .. tostring(v) .. "\n")
        end
      end
    end
    io.write("}\n")
  else
    io.write(tostring(t) .. "\n")
  end
end

local hour = 3600
local minute = 60
local second = 1
local millisecond = 0.001
local microsecond = 0.000001

local toHour = 1 / hour
local toMinute = 1 / minute
local toSecond = 1 / second
local toMillisecond = 1 / millisecond
local toMicrosecond = 1 / microsecond

--- Formats seconds into a human-readable string.
---@param seconds number
function stringh.formatTime(seconds)
  if seconds > hour then
    return string.format("%.2fh", seconds * toHour)
  elseif seconds > minute then
    return string.format("%.2fm", seconds * toMinute)
  elseif seconds > second then
    return string.format("%.2fs", seconds * toSecond)
  elseif seconds > millisecond then
    return string.format("%.2fms", seconds * toMillisecond)
  else
    return string.format("%.2fµs", seconds * toMicrosecond)
  end
end

local kb = 1000
local mb = 1000 * kb
local gb = 1000 * mb
local tb = 1000 * gb

local kib = 1024
local mib = 1024 * kib
local gib = 1024 * mib
local tib = 1024 * gib

local toKB = 1 / kb
local toMB = 1 / mb
local toGB = 1 / gb
local toTB = 1 / tb

local toKiB = 1 / kib
local toMiB = 1 / mib
local toGiB = 1 / gib
local toTiB = 1 / tib

--- Formats a byte count into a human-readable string.
---@param bytes number
---@param binary boolean If true, will use binary prefixes (KiB, MiB, etc.) instead of decimal (KB, MB, etc.).
function stringh.formatBytes(bytes, binary)
  if binary then
    if bytes > tib then
      return string.format("%.2fTiB", bytes * toTiB)
    elseif bytes > gib then
      return string.format("%.2fGiB", bytes * toGiB)
    elseif bytes > mib then
      return string.format("%.2fMiB", bytes * toMiB)
    elseif bytes > kib then
      return string.format("%.2fKiB", bytes * toKiB)
    else
      return string.format("%dB", bytes)
    end
  else
    if bytes > tb then
      return string.format("%.2fTB", bytes * toTB)
    elseif bytes > gb then
      return string.format("%.2fGB", bytes * toGB)
    elseif bytes > mb then
      return string.format("%.2fMB", bytes * toMB)
    elseif bytes > kb then
      return string.format("%.2fKB", bytes * toKB)
    else
      return string.format("%dB", bytes)
    end
  end
end

return stringh
