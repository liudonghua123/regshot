@ECHO OFF

@rem ***************************************************************************
@rem * Copyright 2012 Maddes
@rem * Copyright 2012 Regshot Team
@rem *
@rem * This file is part of Regshot.
@rem *
@rem * Regshot is free software: you can redistribute it and/or modify
@rem * it under the terms of the GNU Lesser General Public License as published by
@rem * the Free Software Foundation, either version 2.1 of the License, or
@rem * (at your option) any later version.
@rem *
@rem * Regshot is distributed in the hope that it will be useful,
@rem * but WITHOUT ANY WARRANTY; without even the implied warranty of
@rem * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@rem * GNU Lesser General Public License for more details.
@rem *
@rem * You should have received a copy of the GNU Lesser General Public License
@rem * along with Regshot.  If not, see <http://www.gnu.org/licenses/>.
@rem *
@rem *
@rem *  make_packages.bat
@rem *    Batch file for building Regshot with WDK and creating the zip packages
@rem *
@rem ***************************************************************************

@rem Helpful links:
@rem http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/batch.mspx

@rem *** Make all changes local to the script, switch to the script's directory and set generic variables
SETLOCAL
CD /D %~dp0

SET LANG=EN
svn status >svn_status.txt
svn diff >svn_diff.txt

@rem *** Jump point (GOTO) to cleanly end script
:End
ENDLOCAL
EXIT /B
