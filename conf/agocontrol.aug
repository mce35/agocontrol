module Agocontrol = autoload xfm

let comment    = IniFile.comment "#" "#"
let sep        = IniFile.sep "=" "="
let entry   = IniFile.indented_entry IniFile.entry_re sep comment
let title   = IniFile.indented_title IniFile.record_re
let record  = IniFile.record title entry

let lns     = IniFile.lns record comment

let filter = (incl "/etc/opt/agocontrol/conf.d/*.conf")

let xfm = transform lns filter

