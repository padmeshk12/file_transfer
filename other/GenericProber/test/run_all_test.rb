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


#############################################################
# Main
#############################################################
begin
  utils = File.expand_path("../scripts/utils.rb", __FILE__)
  load utils
  check_current_smartest8_version
  generic_prober = File.expand_path("../../", __FILE__)
  prober_list = ["TEL", "TSK"] 
  prober_list.each do |prober|
    test = generic_prober + "/" + prober + "/test/runtest.rb"
    if File.exists?(test)
      puts "============ Test for #{prober} =============="
      system(test)
      puts ""
    end
  end
    
rescue => ex
  puts ex.message
  exit 1
end
