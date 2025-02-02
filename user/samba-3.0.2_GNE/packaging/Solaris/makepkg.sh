#!/bin/sh
#
# Copyright (C) Shirish A Kalele 2000
#
# Builds a Samba package from the samba distribution. 
# By default, the package will be built to install samba in /usr/local
# Change the INSTALL_BASE variable to change this: will modify the pkginfo 
# and samba.server files to point to the new INSTALL_BASE
#
INSTALL_BASE=/usr/local

add_dynamic_entries() 
{
  # Add the binaries, docs and SWAT files

  echo "#\n# Binaries \n#"
  cd $DISTR_BASE/source/bin
  for binfile in *
  do
    if [ -f $binfile ]; then
	case $file in 
	CP*.so)
	 echo echo f none samba/lib/charset/$binfile=source/bin/$binfile 0755 root other
	 ;;
	*)
         echo f none samba/bin/$binfile=source/bin/$binfile 0755 root other
	;;
	esac
    fi
  done

  # Add the scripts to bin/
  echo "#\n# Scripts \n#"
  cd $DISTR_BASE/source/script
  for shfile in *
  do
    if [ -f $shfile ]; then
 	echo f none samba/bin/$shfile=source/script/$shfile 0755 root other
    fi
  done

 # add libraries to /lib for winbind
  echo "#\n# Libraries \n#"
    if [ -f $DISTR_BASE/source/nsswitch/libnss_winbind.so ] ; then
 	echo f none /usr/lib/libnss_winbind.so=source/nsswitch/libnss_winbind.so 0755 root other
 	echo s none /usr/lib/libnss_winbind.so.1=/usr/lib/libnss_winbind.so 0755 root other
 	echo s none /usr/lib/libnss_winbind.so.2=/usr/lib/libnss_winbind.so 0755 root other
 	echo s none /usr/lib/nss_winbind.so.1=/usr/lib/libnss_winbind.so 0755 root other
 	echo s none /usr/lib/nss_winbind.so.2=/usr/lib/libnss_winbind.so 0755 root other
    fi

 # add the .dat codepages
  echo "#\n# Codepages \n#"
    for file in $DISTR_BASE/source/codepages/*.dat ; do
      bfile=`basename $file`
      echo f none /usr/local/samba/lib/$bfile=source/codepages/$bfile
    done

  # Add the manpages
  echo "#\n# man pages \n#"
  echo d none /usr ? ? ?
  echo d none /usr/share ? ? ?
  echo d none /usr/share/man ? ? ?

  # Create directories for man page sections if nonexistent
  cd $DISTR_BASE/docs/manpages
  for i in 1 2 3 4 5 6 7 8 9
  do
     manpages=`ls *.$i 2>/dev/null`
     if [ $? -eq 0 ]
     then
	echo d none /usr/share/man/man$i ? ? ?
	for manpage in $manpages 
	do
		echo f none /usr/share/man/man${i}/${manpage}=docs/manpages/$manpage 0644 root other
	done
     fi
  done

  echo "#\n# HTML documentation \n#"
  cd $DISTR_BASE
  list=`find docs/htmldocs -type d | grep -v "/CVS$"`
  for docdir in $list
  do
    if [ -d $docdir ]; then
	echo d none samba/$docdir 0755 root other
    fi
  done

  list=`find docs/htmldocs -type f | grep -v /CVS/`
  for htmldoc in $list
  do
    if [ -f $htmldoc ]; then
      echo f none samba/$htmldoc=$htmldoc 0644 root other
    fi
  done

  # Create a symbolic link to the Samba book in docs/ for beginners
  echo 's none samba/docs/samba_book=htmldocs/using_samba'

  echo "#\n# SWAT \n#"
  cd $DISTR_BASE
  list=`find swat -type d | grep -v "/CVS$"`
  for i in $list
  do
    echo "d none samba/$i 0755 root other"
  done
  list=`find swat -type f | grep -v /CVS/`
  for i in $list
  do
    echo "f none samba/$i=$i 0644 root other"
  done
  # add the .msg files for SWAT
  echo "#\n# msg files \n#"
    for file in $DISTR_BASE/source/po/*.msg ; do
      bfile=`basename $file`
      echo f none /usr/local/samba/lib/$bfile=source/po/$bfile
    done

  echo "#\n# HTML documentation for SWAT\n#"
  cd $DISTR_BASE/docs/htmldocs
  for htmldoc in *
  do
    if [ -f $htmldoc ]; then
      echo f none samba/swat/help/$htmldoc=docs/htmldocs/$htmldoc 0644 root other
    fi
  done

  echo "#\n# Using Samba Book files for SWAT\n#"
  cd $DISTR_BASE/docs/htmldocs

# set up a symbolic link instead of duplicating the book tree
  echo 's none samba/swat/using_samba=../docs/htmldocs/using_samba'

}

if [ $# = 0 ]
then
	# Try to guess the distribution base..
	CURR_DIR=`pwd`
	DISTR_BASE=`echo $CURR_DIR | sed 's|\(.*\)/packaging.*|\1|'`
	echo "Assuming Samba distribution is rooted at $DISTR_BASE.."
else
	DISTR_BASE=$1
fi

#
if [ ! -d $DISTR_BASE ]; then
	echo "Source build directory $DISTR_BASE does not exist."
	exit 1
fi

# Set up the prototype file from prototype.master
if [ -f prototype ]; then
	rm prototype
fi

# Setup version from version.h
VERSION=3.0.2
sed -e "s|__VERSION__|$VERSION|" -e "s|__ARCH__|`uname -p`|" -e "s|__BASEDIR__|$INSTALL_BASE|g" pkginfo.master >pkginfo

sed -e "s|__BASEDIR__|$INSTALL_BASE|g" inetd.conf.master >inetd.conf
sed -e "s|__BASEDIR__|$INSTALL_BASE|g" samba.server.master >samba.server

cp prototype.master prototype

# Add the dynamic part to the prototype file
(add_dynamic_entries >> prototype)

# Create the package
pkgmk -o -d /tmp -b $DISTR_BASE -f prototype
if [ $? = 0 ]
then
	pkgtrans /tmp samba.pkg samba
fi
echo The samba package is in /tmp
