var fs = require("fs")

var contents = fs.readFileSync("intro.c")
var lines = contents.toString().split("\n")
var noteRegex = /NOTE\((.*)\)/
var out = ""

lines.forEach(function(line)
{
    var matches = line.match(noteRegex)
    if (matches)
    {
        var key = parseInt(matches[1])
        var frequency = Math.pow(1.05946309436, key - 49.0) * 440.0
        var period = (44100.0 / frequency) | 0
        
        line = line.replace(noteRegex, period)
    }
    
    out += line + "\n";
})

fs.writeFileSync("out/intro.c", out)
