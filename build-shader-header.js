var fs = require("fs")

var contents = fs.readFileSync("out/shader.min.glsl")
var lines = contents.toString().split("\n")
var shader = lines[lines.length - 1]
var headerContents = ""
headerContents += "#define STRINGIFY(x) #x\n"
headerContents += "#define TOSTRING(x) STRINGIFY(x)\n"
headerContents += "const char *fs = \"" + shader + "\";\n"
headerContents = headerContents.replace(/UNIFORM_COUNT/g, "\"TOSTRING(UNIFORM_COUNT)\"")
headerContents = headerContents.replace(/WIDTH/g, "\"TOSTRING(WIDTH)\"")
headerContents = headerContents.replace(/HEIGHT/g, "\"TOSTRING(HEIGHT)\"")
fs.writeFileSync("out/shader.h", headerContents)
