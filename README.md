# sdk_http
GMod 9 Asynchronous HTTP Lib

Example:

```lua
--HTTP GET
http.Get({["url"] = "http://ident.me"},function(body,code)
	Msg(string.format("http.Get:\nBody: %s\nHTTP Code: %d\n\n",body,code))
end,function(code)
	Msg(string.format("http.Get:\nCURL Error %d\n\n",code))
end)

--HTTP POST
http.Post(
	{
		["url"] = "http://localhost/test/post.php",
		["args"] = "a=yes&b=loles"
	},
	function(body,code)
		Msg(string.format("http.Post:\nResult: %s\n\n",body));
	end,function(code)
		Msg(string.format("http.Post:\nCURL Error %d\n\n",code))
	end
)

--HTTP POST multipart/form-data
http.PostMultipart({
	["url"] = "http://localhost/test/post_multipart_file.php",
	["form"] = {
		["arg"] = "value",
	}
},function()end,function()end)

--HTTP POST multipart/form-data (upload file)
http.PostMultipart(
	{
		["url"] = "http://localhost/test/post_multipart_file.php",
		["file"] = {
			{"file1","test.txt","A veeery long file"}
		}
	},
	function(body,code)
		Msg(string.format("http.PostMultipart:\n%s\n\n",body))
	end,function(code)
		Msg(string.format("http.PostMultipart:\nCURL Error %d\n\n",code))
	end
)
```
