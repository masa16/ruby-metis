
if !File.directory?("metis-5.0pre2")
  if !File.file?("metis-5.0pre2.tar.gz")
    system("wget http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/metis-5.0pre2.tar.gz") || raise
  end
  system("tar xvzf metis-5.0pre2.tar.gz") || raise
  system("patch -p0 < patch-metis-5.0pre2") || raise
end

Dir.chdir("metis-5.0pre2") do
  system("make all usegdb=1 usefpic=1") || raise
end
