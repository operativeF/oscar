#
# awk script to extract build identification from build_number.h, git_info.h, and version.h
# for use by Inno Setup in building installation file for OSCAR.  See DEPLOY.BAT for information.
#
# Usage: gawk -f getBuildInfo.awk build_number.h >buildInfo.iss
#        gawk -f getBuildInfo.awk git_info.h >>buildInfo.iss
#        gawk -f getBuildInfo.awk version.h >>buildInfo.iss
#        echo %cd% | gawk -f %sourcedir%getBuildInfo.awk >>buildInfo.iss

/ build_number / { print "#define MyBuildNumber \"" substr($5,1,length($5)-1) "\""}

/const QString GIT_BRANCH/ { print "#define MyGitBranch \"" substr($3,13,length($3)-14) "\"" }
/const QString GIT_REVISION/ { print "#define MyGitRevision \"" substr($3,15,length($3)-16) "\"" }

/const int major_version / { print "#define MyMajorVersion \"" substr($5,1,length($5)-1) "\""}
/const int minor_version / { print "#define MyMinorVersion \"" substr($5,1,length($5)-1) "\""}
/const int revision_number / { print "#define MyRevision \"" substr($5,1,length($5)-1) "\""}
/const QString ReleaseStatus/ { print "#define MyReleaseStatus " substr($5,1,length($5)-1) }

/32.*bit/ { print "#define MyPlatform \"Win32\"" }
/64.*bit/ { print "#define MyPlatform \"Win64\"" }
