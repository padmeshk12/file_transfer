#! /usr/bin/ruby -W0
#------------------------------------------------------------
# 
# CREATED:  2022
#
# CONTENTS: Main function
#
# AUTHORS:  Zoyi Yu
#
#------------------------------------------------------------

OPT_DIR = '/opt/hp93000'.freeze
OPT_LINK = "#{OPT_DIR}/soc".freeze

#############################################################
# check smartest 8 version
#############################################################
def check_current_smartest8_version
  unless File.exists?(OPT_LINK)
    raise "ERROR: #{OPT_LINK} does not exist."
  end
  if File.symlink?(OPT_LINK) && File.directory?(OPT_LINK)
    current_version =  File.basename(File.readlink(OPT_LINK))
    unless current_version.gsub(/soc64_/, "")[0] == "8"
      raise "ERROR: Current smartest version is: #{current_version}, please switch to smartest 8."
    end
  else
    raise "ERROR: #{OPT_LINK} is not a symbolic link. Unable to get current smartest version."
  end
end


#############################################################
# Main
#############################################################
begin
  check_current_smartest8_version
  
  generic_handler = File.expand_path("../../", __FILE__)
  
  handler_list = ["Advantest", "Aetrium","Atom", "Delta", "DLH", "Ismeca", "MCT", "Mirae", "ASM", \
                  "Multitest", "OSAI", "Rasco", "SPEA", "SRM", "Esmo", "Pyramid", \
                  "Seiko-Epson", "Synax", "TechWing", "Tesec", "YAC", "Yokogawa"]
    
  handler_list.each do |handler|
    test = generic_handler + "/" + handler + "/test/runtest"
    if File.exists?(test)
      puts "============ Test for #{handler} =============="
      system(test)
      puts ""
    end
  end
    
rescue => ex
  puts ex.message
  exit 1
end