MySQL = {
	basepath = PathDir(ModuleFilename()),
	
	OptFind = function (name, required)
		local function check_compile_include(settings, filename)
			if CTestCompile(settings, "#include <" .. filename .. ">\nint main(){return 0;}", "") then
				return true
			end

			return false
		end

		local check = function(option, settings)
			option.value = false
			option.use_syslib = false
			option.use_winlib = 0
			option.lib_path = nil
			
			if check_compile_include(settings, "cppconn/config.h") then
				option.value = true
				option.use_syslib = true
			end
				
			if platform == "win32" then
				option.value = true
				option.use_winlib = 32
			elseif platform == "win64" then
				option.value = true
				option.use_winlib = 64
			end
		end
		
		local apply = function(option, settings)
			if option.use_syslib == true then
				settings.link.libs:Add("mysqlcppconn")
			elseif option.use_winlib > 0 then
				settings.cc.includes:Add(MySQL.basepath .. "/include")
				if option.use_winlib == 32 then
					settings.link.libpath:Add(MySQL.basepath .. "/lib32")
				else
					settings.link.libpath:Add(MySQL.basepath .. "/lib64")
				end
				settings.link.libs:Add("mysqlcppconn")
			end
		end
		
		local save = function(option, output)
			output:option(option, "value")
			output:option(option, "use_syslib")
			output:option(option, "use_winlib")
		end
		
		local display = function(option)
			if option.value == true then
				if option.use_syslib == true then return "using system libaries" end
				if option.use_winlib == 32 then return "using supplied win32 libraries" end
				if option.use_winlib == 64 then return "using supplied win64 libraries" end
				return "using unknown method"
			else
				if option.required then
					return "not found (required)"
				else
					return "not found (optional)"
				end
			end
		end
		
		local o = MakeOption(name, 0, check, save, display)
		o.Apply = apply
		o.include_path = nil
		o.lib_path = nil
		o.required = required
		return o
	end
}
