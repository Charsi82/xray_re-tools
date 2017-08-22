#ifndef _MFnArrayAttrsData
#define _MFnArrayAttrsData
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
// CLASS:    MFnArrayAttrsData
//
// ****************************************************************************

#if defined __cplusplus

// ****************************************************************************
// INCLUDED HEADER FILES


#include <maya/MFnData.h>

// ****************************************************************************
// DECLARATIONS

class MVectorArray;
class MDoubleArray;
class MIntArray;
class MStringArray;

// ****************************************************************************
// CLASS DECLARATION (MFnArrayAttrsData)

//! \ingroup OpenMaya MFn
//! \brief Function set for multiple arrays of attributes for dependency node data. 
/*!
  MFnArrayAttrsData allows the creation and manipulation of multiple
  arrays of attributes as a data object over a single connection for use
  as dependency graph data.

  If a user written dependency node either accepts or produces
  MFnArrayAttrsData, then this class is used to extract or create
  the data that comes from or goes to other dependency graph nodes.
  The MDataHandle::type method will return kDynArrayAttrsData when data
  of this type is present. To access it, the MDataHandle::data() method
  is used to get an MObject for the data and this should then be used
  to initialize an instance of MFnArrayAttrsData.

  NOTE: these data attributes are not storable.
*/
class OPENMAYA_EXPORT MFnArrayAttrsData : public MFnData
{
	declareMFn( MFnArrayAttrsData, MFnData );

public:

	enum Type {
	kInvalid,
	//! use vectorArray() method to extract the attribute array.
	kVectorArray,
	//! use doubleArray() method to extract the attribute array.
	kDoubleArray,
	//! use intArray() method to extract the attribute array.
	kIntArray,
	//! use stringArray() method to extract the attribute array.
	kStringArray,
	kLast
	};

	MStatus			clear();

	unsigned int		count() const;

	MStringArray	list( MStatus *ReturnStatus = NULL) const;

	bool			checkArrayExist( const MString attrName,
									 MFnArrayAttrsData::Type &arrayType,
									 MStatus *ReturnStatus = NULL);

	MVectorArray	vectorArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MDoubleArray	doubleArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MIntArray		intArray( const MString attrName,
									MStatus *ReturnStatus = NULL );
	MStringArray	stringArray( const MString attrName,
									MStatus *ReturnStatus = NULL );

	MObject			create( MStatus *ReturnStatus = NULL );

	MVectorArray     getVectorData( const MString attrName, MStatus *ReturnStatus = NULL);

	MDoubleArray     getDoubleData( const MString attrName, MStatus *ReturnStatus = NULL);

	MIntArray        getIntData( const MString attrName, MStatus *ReturnStatus = NULL);

	MStringArray     getStringData( const MString attrName, MStatus *ReturnStatus = NULL);

BEGIN_NO_SCRIPT_SUPPORT:

	//!	NO SCRIPT SUPPORT
 	declareMFnConstConstructor( MFnArrayAttrsData, MFnData );

END_NO_SCRIPT_SUPPORT:

protected:
// No protected members

private:
// No private members
};

#endif /* __cplusplus */
#endif /* _MFnArrayAttrsData */
