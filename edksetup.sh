#
# Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
# 
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
# In *inux environment, the build tools's source is required and need to be compiled
# firstly, please reference https://edk2.tianocore.org/unix-getting-started.html to 
# to get how to setup build tool.
#
# After build tool is downloaded and compiled, a soft symbol linker need to be created
# at <workspace>/Conf. For example: ln -s /work/BaseTools /work/edk2/Conf/BaseToolsSource.
#
# Setup the environment for unix-like systems running a bash-like shell.
# This file must be "sourced" not merely executed. For example: ". edksetup.sh"
#
# CYGWIN users: Your path and filename related environment variables should be
# set up in the unix style.  This script will make the necessary conversions to
# windows style.
#
# Please reference edk2 user manual for more detail descriptions at https://edk2.tianocore.org/files/documents/64/494/EDKII_UserManual.pdf
#

function HelpMsg()
{
  echo Please note: This script must be \'sourced\' so the environment can be changed.
  echo ". edksetup.sh" 
  echo "source edksetup.sh"
}

function SetupEnv()
{
  if [ -z "$WORKSPACE" ]
  then
    . BaseTools/BuildEnv $*
  else
    . $WORKSPACE/BaseTools/BuildEnv $*
  fi
}

function SourceEnv()
{
if [ \
     "$1" = "-?" -o \
     "$1" = "-h" -o \
     "$1" = "--help" \
   ]
then
  HelpMsg
  return
else
  SetupEnv "$*"
fi
}
if [ $# -gt 1 ] 
then
  HelpMsg
  return
elif [ $# -eq 1 ] && [ "$1" != "BaseTool" ]
then
  HelpMsg
  return
fi 
SourceEnv "$*"

