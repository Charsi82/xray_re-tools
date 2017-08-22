#ifndef _MAnimCurveChange
#define _MAnimCurveChange
//-
// ==========================================================================
// Copyright (C) 1995 - 2006 Autodesk, Inc., and/or its licensors.  All
// rights reserved.
//
// The coded instructions, statements, computer programs, and/or related
// material (collectively the "Data") in these files contain unpublished
// information proprietary to Autodesk, Inc. ("Autodesk") and/or its
// licensors,  which is protected by U.S. and Canadian federal copyright law
// and by international treaties.
//
// The Data may not be disclosed or distributed to third parties or be
// copied or duplicated, in whole or in part, without the prior written
// consent of Autodesk.
//
// The copyright notices in the Software and this entire statement,
// including the above license grant, this restriction and the following
// disclaimer, must be included in all copies of the Software, in whole
// or in part, and all derivative works of the Software, unless such copies
// or derivative works are solely in the form of machine-executable object
// code generated by a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// AUTODESK DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE,
// OR ARISING FROM A COURSE OF DEALING, USAGE, OR TRADE PRACTICE. IN NO
// EVENT WILL AUTODESK AND/OR ITS LICENSORS BE LIABLE FOR ANY LOST
// REVENUES, DATA, OR PROFITS, OR SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES, EVEN IF AUTODESK AND/OR ITS LICENSORS HAS
// BEEN ADVISED OF THE POSSIBILITY OR PROBABILITY OF SUCH DAMAGES.
// ==========================================================================
//+
//
// CLASS:    MAnimCurveChange
//
// ****************************************************************************

#if defined __cplusplus

// ****************************************************************************
// INCLUDED HEADER FILES


#include <maya/MStatus.h>
#include <maya/MTypes.h>

// ****************************************************************************
// CLASS DECLARATION (MAnimCurveChange)

//! \ingroup OpenMayaAnim
//! \brief  Anim Curve Change Cache
/*!
  Adding, removing and changing keyframes on an anim curve cannot be done
  simply by setting attribute values on the corresponding node. This makes it
  impossible to capture such changes in an MDGModifier for undo/redo purposes.

  The MAnimCurveChange class provides persistent storage for
  information concerning changes to anim curve nodes, along with methods to
  undo and redo those changes. MFnAnimCurve methods which add, remove or
  change keyframes take an optional MAnimCurveChange instance in which to
  store information about the changes being made.

  If the same MAnimCurveChange instance is used for multiple anim curve edit
  operations, then the cache maintains an undo/redo queue which allows all
  changes in the series to be undone or redone.  If selective undo/redo is
  required, then a different MAnimCurveCache instance must be used for each
  edit.
*/
class OPENMAYAANIM_EXPORT MAnimCurveChange
{

public:
	MAnimCurveChange( MStatus * ReturnStatus = NULL );
	~MAnimCurveChange();
	MStatus undoIt();
	MStatus redoIt();

	bool isInteractive() const;
	void setInteractive(bool value);

	static const char* className();

protected:
// No protected members

private:
	void*		 data;
};

#endif /* __cplusplus */
#endif /* _MAnimCurveChange */
