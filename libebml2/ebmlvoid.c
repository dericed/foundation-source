/*
 * $Id$
 * Copyright (c) 2008, Matroska Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Matroska Foundation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY The Matroska Foundation ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL The Matroska Foundation BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "ebml/ebml.h"

static bool_t IsDefaultValue(const ebml_element *Element)
{
    return 0;
}

static filepos_t UpdateSize(ebml_element *Element, bool_t bWithDefault, bool_t bForceRender)
{
    return Element->DataSize;
}

#if defined(CONFIG_EBML_WRITING)
static err_t RenderData(ebml_element *Element, stream *Output, bool_t bForceRender, bool_t bWithDefault, filepos_t *Rendered)
{
    size_t Written, Left = (size_t)Element->DataSize;
    err_t Err = ERR_NONE;
    uint8_t Buf[2*1024]; // write 2 KB chunks at a time
    memset(Buf,0,sizeof(Buf));
    while (Err==ERR_NONE && Left)
    {
        Err = Stream_Write(Output,Buf,min(Left,sizeof(Buf)),&Written);
        if (Err == ERR_NONE)
            Left -= min(Left,sizeof(Buf));
    }
    if (Rendered)
        *Rendered = Element->DataSize - Left;
    return Err;
}
#endif

META_START(EBMLVoid_Class,EBML_VOID_CLASS)
META_VMT(TYPE_FUNC,ebml_element_vmt,IsDefaultValue,IsDefaultValue)
META_VMT(TYPE_FUNC,ebml_element_vmt,UpdateSize,UpdateSize)
#if defined(CONFIG_EBML_WRITING)
META_VMT(TYPE_FUNC,ebml_element_vmt,RenderData,RenderData)
#endif
META_END(EBML_ELEMENT_CLASS)

#if defined(CONFIG_EBML_WRITING)
void EBML_VoidSetSize(ebml_element *Void, filepos_t DataSize)
{
    assert(Node_IsPartOf(Void,EBML_VOID_CLASS));
    Void->DataSize = DataSize;
    Void->bValueIsSet = 1;
}

filepos_t EBML_VoidReplaceWith(ebml_element *Void, ebml_element *ReplacedWith, stream *Output, bool_t ComeBackAfterward, bool_t bWithDefault)
{
    filepos_t CurrentPosition;
    assert(Node_IsPartOf(Void,EBML_VOID_CLASS));

	EBML_ElementUpdateSize(ReplacedWith,bWithDefault,0);
	if (EBML_ElementFullSize(Void,1) < EBML_ElementFullSize(ReplacedWith,1))
		// the element can't be written here !
		return INVALID_FILEPOS_T;
	if (EBML_ElementFullSize(Void,1) - EBML_ElementFullSize(ReplacedWith,1) == 1)
		// there is not enough space to put a filling element
		return INVALID_FILEPOS_T;

	CurrentPosition = Stream_Seek(Output,0,SEEK_CUR);

    Stream_Seek(Output,Void->ElementPosition,SEEK_SET);
    EBML_ElementRender(ReplacedWith,Output,bWithDefault,0,1,NULL,0);

    if (EBML_ElementFullSize(Void,1) - EBML_ElementFullSize(ReplacedWith,1) > 1)
    {
        // fill the rest with another void element
        ebml_element *aTmp = EBML_ElementCreate(Void,Void->Context,0,NULL);
        if (aTmp)
        {
            filepos_t HeadBefore,HeadAfter;
            EBML_VoidSetSize(aTmp, EBML_ElementFullSize(Void,1) - EBML_ElementFullSize(ReplacedWith,1) - 1); // 1 is the length of the Void ID
            HeadBefore = EBML_ElementFullSize(aTmp,1) - aTmp->DataSize;
            aTmp->DataSize = aTmp->DataSize - EBML_CodedSizeLength(aTmp->DataSize, aTmp->SizeLength, EBML_ElementIsFiniteSize(aTmp));
            HeadAfter = EBML_ElementFullSize(aTmp,1) - aTmp->DataSize;
            if (HeadBefore != HeadAfter)
                aTmp->SizeLength = (int8_t)(EBML_CodedSizeLength(aTmp->DataSize, aTmp->SizeLength, EBML_ElementIsFiniteSize(aTmp)) - (HeadAfter - HeadBefore));
            EBML_ElementRenderHead(aTmp,Output,0,NULL);
            NodeDelete((node*)aTmp);
        }
    }

	if (ComeBackAfterward)
        Stream_Seek(Output,CurrentPosition,SEEK_SET);

	return EBML_ElementFullSize(Void,1);
}
#endif
