// psd2anim (based on psdlite - Copyright (c) 2007, Nils Jonas Norberg)
// modified by joric (animation export)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <vector>

#define VA_FCC(sig) (sig >> 24), (sig >> 16), (sig >> 8), (sig)

#define LogDebug(x,...) false
#define LogParse(x,...) false
#define LogStdio(x,...) false

#define LogDebug printf
#define LogParse printf
#define LogStdio printf

char *indent(int lvl)
{
    const int buf_size = 256;
    static char buf[buf_size];
    sprintf(buf, "");
    while (lvl-- && lvl < buf_size)
        strcat(buf, "\t");
    return buf;
}

int round_int(int offset, int align)
{
    return offset + ((align - (offset % align)) % align);
}

namespace psdlite {

    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned int u32;

    typedef char s8;
    typedef short s16;
    typedef int s32;

    struct vi2 {
        vi2():x(0), y(0) {
        } vi2(int w, int h) {
            set(w, h);
        } void set(int w, int h) {
            x = w;
            y = h;
        }
        int x, y;
    };

    struct pixel {
        static const u32 CHANNELS = 4;
          pixel() {
            for (u32 i = 0; i != CHANNELS; ++i)
                v[i] = 128;
        };

        u8 a() {
            return v[0];
        }
        u8 r() {
            return v[1];
        }
        u8 g() {
            return v[2];
        }
        u8 b() {
            return v[3];
        }

        u8 v[CHANNELS];
    };

    struct bitmap {
        bitmap() {
        } bitmap(int width, int height) {
            resize(width, height);
        }

        void resize(int width, int height) {
            size_.set(width, height);
            data_.resize(width * height);
        }

        const pixel get_pixel(int x, int y) const {
            if (0 <= x && x < size_.x && 0 <= y && y < size_.y)
                return data_[y * size_.x + x];
            return pixel();
        } void set_pixel(int x, int y, const pixel & p) {
            if (0 <= x && x < size_.x && 0 <= y && y < size_.y)
                data_[y * size_.x + x] = p;
        }

        void set_single_channel(int x, int y, u32 channel, u8 v) {
            if (0 <= x && x < size_.x && 0 <= y && y < size_.y && channel < pixel::CHANNELS)
                data_[y * size_.x + x].v[channel] = v;
        }

        void set_channel_count(u32 channel_count) {
            channel_count_ = channel_count;
        }

        u32 get_channel_count() {
            return channel_count_;
        }

        const vi2 & get_size() const {
            return size_;
} private:
          std::vector < pixel > data_;
        vi2 size_;
        u32 channel_count_;
    };

    struct animation {
        vi2 offs_;
        int enabled;
    };

    struct layer {
        std::string name_;
        vi2 offs_;
        bitmap data_;
        int flags;
    };

    struct layered_image {
        vi2 size_;
          std::vector < layer > layers_;
    };

    enum error_code {
        error_code_no_error,
        error_code_not_supported,
        error_code_invalid_file,
    };

    error_code load_layered_image(layered_image & dest, const char *fname);
}

namespace psdlite {
    struct buffered_file {
        buffered_file(const char *fname):iter_(0) {
            mem_.clear();
            FILE *f = fopen(fname, "rb");
            if (!f)
                  return;

              fseek(f, 0, SEEK_END);
              mem_.resize(ftell(f));
              fseek(f, 0, SEEK_SET);
              fread(&mem_[0], mem_.size(), 1, f);
              fclose(f);
        } size_t get_pos() {
            return iter_;
        } void set_pos(size_t pos) {
            iter_ = pos;
            if (iter_ > mem_.size())
            {
                throw error_code_invalid_file;
            }
        }

        void pad_even() {
            iter_ = (iter_ + 1) & ~1;
            if (iter_ > mem_.size())
            {
                throw error_code_invalid_file;
            }
        }

        u32 getKey() {
            u32 len = getu32();
            if (len == 0)
                len = getu32();
            else
                skip(len);
            return len;
        }

        s32 gets32() {
            return (s32) getu32();
        }

        u32 getu32() {
            if (iter_ + 4 > mem_.size())
            {
                throw error_code_invalid_file;
            }

            u8 *p = (u8 *) & mem_[iter_];
            iter_ += 4;
            return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
        }

        u16 getu16() {
            if (iter_ + 2 > mem_.size())
            {
                throw error_code_invalid_file;
            }

            u8 *p = (u8 *) & mem_[iter_];
            iter_ += 2;
            return (p[0] << 8) | p[1];
        }

        s16 gets16() {
            return (s16) getu16();
        }

        u8 getu8() {
            if (iter_ + 1 > mem_.size())
            {
                throw error_code_invalid_file;
            }

            u8 *p = (u8 *) & mem_[iter_];
            iter_ += 1;
            return p[0];
        }

        s8 gets8() {
            return (s8) getu8();
        }

        void skip(u32 bytes) {
            iter_ += bytes;

            if (iter_ > mem_.size())
            {
                throw error_code_invalid_file;
            }
        }

        void skip_pstring() {
            u8 s = getu8();
            skip(s);
            pad_even();
        }

        void skip_ustring() {
            u32 s = getu32();
            skip(s * 2);
        }

        int get_pstring(std::string & str) {
            u8 s = getu8();
            iter_ += s;
            if (iter_ > mem_.size())
            {
                throw error_code_invalid_file;
            }

            str.assign(&mem_[iter_ - s], &mem_[iter_]);
            pad_even();
            return s + 1;
        }

        int getu32p(int ofs) {
            u8 *p = (u8 *) & mem_[iter_];
            p += ofs;
            return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
        }

        void dumpHex(u32 bytes) {
            for (int i = 0; i < bytes; i++)
            {
                unsigned char b = (unsigned char)mem_[iter_ + i];
                unsigned char c = b;
                if (b < 32 || b > 128)
                    c = ' ';
                printf("%04d: %02x %c", i, b, c);
                printf(i == (bytes - 1) ? "\n\n" : "\n");
            }
            iter_ += bytes;
        }

protected:
        std::vector < s8 > mem_;
        size_t iter_;
    };

    /////////////////////////////////////////////////////////////

    struct loader {
        loader(buffered_file & file):file_(file) {
            m_layer = -1;
            m_frame = -1;
        } int m_layer;
        int m_frame;

        void next_layer() {
            m_layer++;
            m_frame = -1;
        }

        void next_frame() {
            m_frame++;
        }

        void set_frame_delay(layered_image & dest, int delay) {
            LogDebug("frame %d, delay: %d\n", m_frame, delay);
        }

        void set_layer_visible(layered_image & dest, int visible) {
            LogDebug("layer: %d, frame %d, visible: %d\n", m_layer, m_frame, visible);
        }

        void set_layer_dx(layered_image & dest, int dx) {
            LogDebug("layer: %d, frame %d, dx: %d\n", m_layer, m_frame, dx);
        }

        void set_frame_layer_dy(layered_image & dest, int dy) {
            LogDebug("layer: %d, frame %d, dy: %d\n", m_layer, m_frame, dy);
        }

private:
        buffered_file & file_;

        void operator=(const loader &) {
        };        // no assignement operator

        void parse_header(layered_image & dest) {
            u32 sig = file_.getu32();
            (void)sig;

            u16 ver = file_.getu16();
            (void)ver;

            file_.skip(6);

            u16 channels = file_.getu16();
            (void)channels;

            u32 rows = file_.getu32();
            u32 columns = file_.getu32();

            dest.size_.set(columns, rows);

            u16 depth = file_.getu16();
            if (depth != 8)
            {
                throw error_code_not_supported;    // only support 8bit depth
            }

            u16 mode = file_.getu16();

            if (mode != 3)
            {
                throw error_code_not_supported;    // only support RGB mode
            }
        }

        void _skip_block() {
            u32 size = file_.getu32();
            file_.dumpHex(size);
        }

        void skip_block() {
            u32 size = file_.getu32();
            file_.skip(size);
        }

        void parse_animation_block_data(layered_image & dest) {

            u32 id = file_.getu32();    //'8BIM'
            u32 type = file_.getu32();    //'AnDs'
            u32 size = file_.getu32();    //size

//            printf("Animation block data: %c%c%c%c %c%c%c%c %d\n", VA_FCC(id), VA_FCC(type), size);

            size_t endpos = file_.get_pos() + size;

            if (size > 0)
            {
                u32 desc = file_.getu32();    //0x0010
                parse_descriptor(0, 0, dest);
            }

            file_.set_pos(endpos);
        }

        void parse_animation_block(layered_image & dest, u32 size) {

            size_t bstart = file_.get_pos();
            size_t endpos = file_.get_pos() + size;

            u32 id = file_.getu32();    //'mani'
            u32 type = file_.getu32();    //'IRFR'
            u32 bsize = file_.getu32();

//            printf("Animation block: %c%c%c%c %c%c%c%c %d\n", VA_FCC(id), VA_FCC(type), bsize);

            parse_animation_block_data(dest);

            file_.set_pos(endpos);
        }

        void parse_animation_metadata(layered_image & dest) {
            u32 size = file_.getu32();

            size_t endpos = file_.get_pos() + size;

            u32 desc = file_.getu32();
            parse_descriptor(0, 0, dest);

            file_.set_pos(endpos);
        }


        void parse_image_resources(layered_image & dest) {

            u32 size = file_.getu32();
            u32 endpos = file_.get_pos() + size;

            while (file_.get_pos() < endpos)
            {
                parse_image_resource_block(dest);
            }

        }

        void parse_image_resource_block(layered_image & dest) {            

            u32 block_type = file_.getu32();
            (void)block_type;

            u16 block_id = file_.getu16();
            (void)block_id;

            file_.skip_pstring();

            u32 size = file_.getu32();

            size_t endpos = file_.get_pos() + size;

//            LogDebug("Image resource block: %c%c%c%c, %04d, %d bytes\n", VA_FCC(block_type), block_id, size);

            switch (block_id)
            {
                case 4000: case 4004:
                    parse_animation_block(dest, size);
                    break;
                    
                default:                
                    file_.skip(size);
                    break;
            }

            file_.pad_even();
        }

        void parseRAWChannel(bitmap & dest, int color_channel) {
            vi2 s = dest.get_size();

            for (int y = 0; y != s.y; ++y)
            {
                for (int x = 0; x != s.x; ++x)
                {
                    u8 v = file_.getu8();
                    dest.set_single_channel(x, y, color_channel, v);
                }
            }
        }

        void parseRLEChannel(bitmap & dest, int color_channel) {
            // RLE compression...
            vi2 s = dest.get_size();

            std::vector < u16 > scanline_byte_counts(s.y);

            // get bytecounts for all scanlines
            for (int y = 0; y != s.y; ++y)
            {
                scanline_byte_counts[y] = file_.getu16();
            }

            for (int y = 0; y != s.y; ++y)
            {
                int line_bytes = scanline_byte_counts[y];
                for (int x = 0; line_bytes && (x < s.x);)
                {
                    int control_byte = file_.gets8();
                    --line_bytes;
                    if (control_byte < 0)
                    {
                        int count = 1 - control_byte;

                        // RLE
                        if (line_bytes)
                        {
                            s8 v = file_.getu8();
                            --line_bytes;

                            for (; count; --count, ++x)
                                dest.set_single_channel(x, y, color_channel, v);
                        }
                    }
                    else
                    {
                        int count = 1 + control_byte;

                        // RAW
                        for (; count && line_bytes; --line_bytes, --count, ++x)
                            dest.set_single_channel(x, y, color_channel, file_.getu8());
                    }
                }
            }
        }

        void parse_layer_channel_data(bitmap & dest) {
            int cc = dest.get_channel_count();
            for (int channel = 0; channel != cc; ++channel)
            {
                int color_channel = channel;

                // if only RGB skip A in ARGB
                if (cc == 3)
                {
                    color_channel++;
                }

                u16 compression = file_.getu16();

                switch (compression)
                {
                    case 0:    // raw data
                        parseRAWChannel(dest, color_channel);
                        break;
                    case 1:    // rle.. good
                        parseRLEChannel(dest, color_channel);
                        break;
                    case 2:
                    case 3:
                        throw error_code_not_supported;
                }
            }
        }

        void parse_descriptor(u32 lvl, u32 node, layered_image & dest) {

            file_.skip_ustring();

            u32 size = file_.getu32();
            u32 id = file_.getu32();
            u32 items = file_.getu32();

            for (int i = 0; i < items; i++)
            {
                u32 key = file_.getKey();
                u32 type = file_.getu32();
                parse_type(lvl, i, node, key, type, dest);
            }
        }

        void parse_object_list(u32 lvl, u32 node, layered_image & dest) {
            u32 items = file_.getu32();
            for (int i = 0; i < items; i++)
            {
                u32 type = file_.getu32();
                parse_type(lvl, i, node, 'VlLs', type, dest);
            }
        }

        void parse_type(u32 lvl, u32 idx, u32 node, u32 id, u32 type, layered_image & dest) {

            LogParse("%s%c%c%c%c:%c%c%c%c\n", indent(lvl), VA_FCC(id), VA_FCC(type));

            u8 vbool = 0;
            s32 vlong = 0;
            double vdouble = 0;

            if (id == 'LaID')
                next_layer();

            if (id == 'VlLs' && lvl == 1)
                next_frame();

            switch (type)
            {
                case 'VlLs':
                    parse_object_list(lvl + 1, id, dest);
                    break;

                case 'Objc':
                case 'GLbO':
                    parse_descriptor(lvl + 1, id, dest);
                    break;

                case 'bool':
                    vbool = file_.getu8();
                    LogParse("%s%d\n", indent(lvl + 1), vbool);
                    break;

                case 'long':
                    vlong = file_.gets32();
                    LogParse("%s%d\n", indent(lvl + 1), vlong);
                    break;

                case 'doub':
                    vdouble = (double)file_.gets32();
                    file_.gets32();
                    LogParse("%s%f\n", indent(lvl + 1), vdouble);
                    break;

                case 'UntF':
                    file_.gets32();
                    file_.gets32();
                    file_.gets32();
                    break;

                case 'TEXT':
                    file_.skip_ustring();
                    break;

                default:
                    file_.gets32();
                    break;
            }

            if (id == 'FrDl')
            {
                set_frame_delay(dest, vlong);
            }

            if (id == 'enab')
            {
                set_layer_visible(dest, vbool);
            }

            if (node == 'Ofst')
            {
                if (id == 'Hrzn')
                    set_layer_dx(dest, vlong);
                if (id == 'Vrtc')
                    set_frame_layer_dy(dest, vlong);
            }
        }

        void parse_metadata(layered_image & dest) {
            u32 size = file_.getu32();
            u32 items = file_.getu32();

            for (int i = 0; i < items; i++)
            {
                u32 sig = file_.getu32();
                u32 key = file_.getu32();
                u32 unk = file_.getu32();

                switch (key)
                {
                    case 'mlst':
                        parse_animation_metadata(dest);
                        break;
                    default:
                        skip_block();
                        break;
                }
            }
        }

        void parse_layer_addinfo(layered_image & dest) {
            u32 sig = file_.getu32();
            u32 key = file_.getu32();

            switch (key)
            {
                case 'shmd':
                    parse_metadata(dest);
                    break;

                default:
                    skip_block();
                    break;
            }
        }

        void parse_layer_pixel_data(layered_image & dest) {
            for (u32 i = 0; i != dest.layers_.size(); ++i)
                parse_layer_channel_data(dest.layers_[i].data_);
        }

        void parse_layer_record(layered_image & dest) {
            u32 top = file_.getu32();
            u32 left = file_.getu32();
            u32 bottom = file_.getu32();
            u32 right = file_.getu32();

            u16 channel_count = file_.getu16();
            file_.skip(6 * channel_count);    // skip channel length info

            u32 blend_mode_sig = file_.getu32();
            (void)blend_mode_sig;

            u32 blend_mode_key = file_.getu32();
            (void)blend_mode_key;

            u8 opacity = file_.getu8();
            (void)opacity;

            u8 clipping = file_.getu8();
            (void)clipping;

            u8 flags = file_.getu8();
            (void)flags;

            u8 filler = file_.getu8();
            (void)filler;

            u32 extra_size = file_.getu32();
            size_t endpos = file_.get_pos() + extra_size;

            skip_block();    // layer mask adjustment data
            skip_block();    // layer blending ranges

            // "Pascal string, padded to a multiple of 4 bytes"
            size_t p0 = file_.get_pos();
            std::string l_name;
            int len = file_.get_pstring(l_name);
            len = round_int(len, 4);
            file_.set_pos(p0 + len);

            while (file_.get_pos() < endpos)
                parse_layer_addinfo(dest);

            file_.set_pos(endpos);

            // add layer to dest
            dest.layers_.push_back(layer());
            layer & l = dest.layers_.back();
            l.name_ = l_name;
            l.offs_.set(left, top);
            l.data_.resize(right - left, bottom - top);
            l.data_.set_channel_count(channel_count);

            l.flags = flags;
        }

        void parse_layer_structure(layered_image & dest) {
            s16 l_count = (s16) abs(file_.gets16());

            for (int i = 0; i != l_count; ++i)
                parse_layer_record(dest);

            file_.pad_even();
        }

        void parse_layer_info(layered_image & dest) {
            u32 size = file_.getu32();

            size_t endpos = file_.get_pos() + size;

            parse_layer_structure(dest);
            parse_layer_pixel_data(dest);

            file_.set_pos(endpos);
        }

        void parse_layer_and_mask(layered_image & dest) {
            u32 size = file_.getu32();
            size_t endpos = file_.get_pos() + size;

            parse_layer_info(dest);
            skip_block();    // parse_mask_info( dest, ptr, stop );

            file_.set_pos(endpos);
        }

public:
        void parse_layered_image(layered_image & dest) {
            parse_header(dest);
            skip_block();    //parse_color_data( dest );
            parse_image_resources(dest);
            parse_layer_and_mask(dest);
            // skip composite image...
        }
    };

    error_code load_layered_image(layered_image & dest, const char *fname) {
        try
        {
            // clear dest
            dest.layers_.clear();
            dest.size_.set(0, 0);

            // load file
            buffered_file file(fname);

            loader l(file);
            l.parse_layered_image(dest);
        }
        catch(error_code e)
        {
            return e;
        }
        catch(...)
        {
            return error_code_invalid_file;
        }

        return error_code_no_error;
    }
}

int main(int argc, char **argv)
{
    using namespace std;
    using namespace psdlite;

    layered_image img;
    const char *filename = (argc >= 2) ? argv[1] : "anim.psd";

    char *basename = strdup(filename);
    //remove extension, if any
    char *pos = strrchr(basename, '.');
    if (pos)
        *pos = 0;

    LogStdio("Loading %s\n", filename);

    int code = load_layered_image(img, filename);

    if (code)
    {
        LogStdio("ERROR: %d\n", code);
        exit(code);
    }

    u32 lc = (u32) img.layers_.size();


    for (u32 i = 0; i != lc; ++i)
    {
        layer & l = img.layers_[i];

        LogStdio("name: '%s' x,y=%d,%d w,h=%d,%d flags=%d\n", 
            l.name_.c_str(), l.offs_.x, l.offs_.y, l.data_.get_size().x, l.data_.get_size().y, l.flags);

        bool hidden = (l.flags & 2);

        if (!hidden)
        {
            bitmap & b = l.data_;
            vi2 s = b.get_size();
            u8* mem = (u8*)malloc(s.x * s.y * 4);
            int count = 0;
            for (int y = 0; y != s.y; ++y)
            {
                for (int x = 0; x != s.x; ++x)
                {
                    pixel p = b.get_pixel(x, y);
                    mem[count++] = p.b();
                    mem[count++] = p.g();
                    mem[count++] = p.r();
                    mem[count++] = p.a();
                }
            }
        }
    }

    return 0;
}

