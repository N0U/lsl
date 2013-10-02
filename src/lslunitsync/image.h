#ifndef LSL_IMAGE_H
#define LSL_IMAGE_H

#include <boost/shared_ptr.hpp>

//we really, really don't want to include the cimg
// header here, it's 2.1MB of template magic :)
namespace cimg_library {
template <class T>
struct CImg;
}

#ifdef HAVE_WX
    class wxBitmap;
    class wxImage;
#endif

namespace LSL {

namespace Util {
template <class T>
class uninitialized_array;
}

/** we use this class mostly to hide the cimg implementation details
 * \todo decide/implement COW
 */
class UnitsyncImage
{
private:
    typedef unsigned short
        RawDataType;
    typedef cimg_library::CImg<RawDataType>
        PrivateImageType;
	typedef boost::shared_ptr<PrivateImageType>
		PrivateImagePtrType;
public:
	UnitsyncImage();
	explicit UnitsyncImage( int width, int height );
	UnitsyncImage( const std::string& filename );

    //! delegates save to cimg library, format is deducted from last path compoment (ie. after the last dot)
	void Save( const std::string& path ) const;
    //! same principle as \ref Save
	void Load( const std::string& path ) const;

  /** \name factory functions
   * \brief creating UnitsyncImage from raw data pointers
   **/
  ///@{
	static UnitsyncImage FromMinimapData( const RawDataType* data, int width, int height );
	static UnitsyncImage FromHeightmapData( const Util::uninitialized_array<unsigned short>& data, int width, int height );
	static UnitsyncImage FromMetalmapData( const Util::uninitialized_array<unsigned char>& data, int width, int height );
	static UnitsyncImage FromVfsFileData( Util::uninitialized_array<char>& data, size_t size, const std::string& fn, bool useWhiteAsTransparent = true );
    ///@}

    #ifdef HAVE_WX
    wxBitmap wxbitmap() const;
    wxImage wximage() const;
    #endif
	int GetWidth() const;
	int GetHeight() const;
	void Rescale( const int new_width, const int new_height);
	//rescale image to a max resolution 512x512 with keeping aspect ratio
	void RescaleIfBigger(const int maxwidth= 512, const int maxheight=512);

	bool isValid() { return ((GetWidth()>0) && (GetHeight()>0));}
	// makes given color transparent
	void MakeTransparent(unsigned short r = 255, unsigned short g = 255, unsigned short b = 255);
private:
	UnitsyncImage( PrivateImagePtrType ptr );
	static PrivateImageType* NewImagePtr( int width = 0, int height = 0 );
	PrivateImagePtrType m_data_ptr;
};

} //namespace LSL

/**
 * \file image.h
 * \section LICENSE
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#endif // LSL_IMAGE_H
