#! /bin/bash
#
if [ $# != 1 ] ; then
   echo $0 requires a SleepyHead data folder name ;
   exit ;
fi
SRC=~/Documents/$1
DEST=~/Documents/OSCAR_Data
#
echo This will create a new folder called $DEST with a copy of the data in $SRC
echo suitably modified to work with OSCAR
#
#echo Copying $SRC to $DEST may take a while if you have many months of data
#cp -r $SRC $DEST
cd $DEST
#
for f in *.xml ; do
   echo Fixing $f ;
   sed -i s/SleepyHead/OSCAR/ $f ;
done
#
cd Profiles

for f in * ; do
   if [[ -d ${f} ]]; then
      echo Entering folder $f ;
      cd "$f" ;
      for ff in *.xml ; do
         echo Fixing $ff ;
         sed -i s/SleepyHead/OSCAR/ $ff ;
      done
      cd ..
   fi
done
#
echo All done!
#
