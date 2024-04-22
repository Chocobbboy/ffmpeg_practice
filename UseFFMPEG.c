#include "myheader.h"
#define INBUF_SIZE 4096
#define ARGV1 1
#define ARGV2 2
#define ARGV3 3
#define ARGV4 4

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename)
{
    FILE *f;
    int i;

    f = fopen(filename,"wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}


static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename)
{
    char buf[1024];
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        printf("avcode_is_open = %d\n", avcodec_is_open(dec_ctx));
        printf("av_codec_is_decoder = %d\n", av_codec_is_decoder(dec_ctx->codec));
        if(pkt){printf("pkt exists, size = %d, is null = %d\n", pkt->size == 0, pkt->data == NULL);}
        
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }

        //printf("saving frame %3"PRId64"\n", dec_ctx->frame_number);
        fflush(stdout);

        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0],
                 frame->width, frame->height, buf);
    }
}

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *outfile)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3"PRId64"\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

int main(int argc, char **argv){
    printf("hello\n");
    const char *filename, *outfilename, *codec_name;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *frame;
    AVPacket *pkt;
    FILE *f;
    int ret;

    if (argc <= 1) {
        fprintf(stderr, "Usage: <d/e> <I/O file name> <O filename/codec name>\n");
        exit(0);
    }else{
        
        switch(*argv[1]){
            case 'd':
	    AVCodecParserContext *parser;
	    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	    uint8_t *data;
	    size_t   data_size;
	    int eof;

	    if (argc <= 3) {
		fprintf(stderr, "Usage: %s <input file> <output file> <codec name>(AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_H264)\n"
		        "And check your input file is encoded by mpeg1video please.\n", argv[0]);
		exit(0);
	    }
	    filename    = argv[ARGV2];
	    outfilename = argv[ARGV3];
	    codec_name  = argv[ARGV4];

	    pkt = av_packet_alloc();
	    if (!pkt)
		exit(1);

	    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	    /* find the MPEG-1 video decoder */
	    if(!strcmp(codec_name, "AV_CODEC_ID_MPEG1VIDEO")){
		codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
	    }else if(!strcmp(codec_name, "AV_CODEC_ID_H264")){
		codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	    }else{
		fprintf(stderr, "not support this codec yet: %s\n", codec_name);
		exit(1);
	    }
	    
	    if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	    }

	    parser = av_parser_init(codec->id);
	    if (!parser) {
		fprintf(stderr, "parser not found\n");
		exit(1);
	    }

	    c = avcodec_alloc_context3(codec);
	    if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	    }

	    /* For some codecs, such as msmpeg4 and mpeg4, width and height
	       MUST be initialized there because this information is not
	       available in the bitstream. */

	    /* open it */
	    if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	    }

	    f = fopen(filename, "rb");
	    if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	    }

	    frame = av_frame_alloc();
	    if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	    }

	    do {
		/* read raw data from the input file */
		data_size = fread(inbuf, 1, INBUF_SIZE, f);
		printf("\ndata_size = %zu\n", data_size);
		if (ferror(f))
		    break;
		eof = !data_size;

		/* use the parser to split the data into frames */
		data = inbuf;
		while (data_size > 0 || eof) {
		    ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
		                           data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		    if (ret < 0) {
		        fprintf(stderr, "Error while parsing\n");
		        exit(1);
		    }
		    data      += ret;
		    data_size -= ret;

		    if (pkt->size){
		        decode(c, frame, pkt, outfilename);
		    }
		    else if (eof)
		        break;
		}
	    } while (!eof);

	    /* flush the decoder */
	    decode(c, frame, NULL, outfilename);
	    fclose(f);

	    av_parser_close(parser);
	    avcodec_free_context(&c);
	    av_frame_free(&frame);
	    av_packet_free(&pkt);
                break;
            case 'e':
                printf("do encode\n");
                int i, x, y;
                uint8_t endcode[] = { 0, 0, 1, 0xb7 };

                if (argc <= 3) {
                    fprintf(stderr, "Usage: %s <output file> <codec name>(AV_CODEC_ID_MPEG1VIDEO, AV_CODEC_ID_H264)\n", argv[0]);
                    exit(0);
                }

                filename = argv[ARGV2];
                codec_name = argv[ARGV3];

                /* find the encoder */
                //codec = avcodec_find_encoder_by_name(codec_name);
                //codec = avcodec_find_encoder(codec_name);
                if(!strcmp(codec_name, "AV_CODEC_ID_MPEG1VIDEO")){
                    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
                }else if(!strcmp(codec_name, "AV_CODEC_ID_H264")){
                    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
                }else{
                    fprintf(stderr, "not support this codec yet: %s\n", codec_name);
                    exit(1);
                }

                c = avcodec_alloc_context3(codec);
                if (!c) {
                    fprintf(stderr, "Could not allocate video codec context\n");
                    exit(1);
                }

                pkt = av_packet_alloc();
                if (!pkt)
                    exit(1);

                /* put sample parameters */
                c->bit_rate = 400000;
                /* resolution must be a multiple of two */
                c->width = 100;
                c->height = 100;
                /* frames per second */
                c->time_base = (AVRational){1, 25};
                c->framerate = (AVRational){25, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
                c->gop_size = 10;
                c->max_b_frames = 1;
                c->pix_fmt = AV_PIX_FMT_YUV420P;

                if (codec->id == AV_CODEC_ID_H264)
                    av_opt_set(c->priv_data, "preset", "slow", 0);

                /* open it */
                ret = avcodec_open2(c, codec, NULL);
                if (ret < 0) {
                    fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
                    exit(1);
                }

                f = fopen(filename, "wb");
                if (!f) {
                    fprintf(stderr, "Could not open %s\n", filename);
                    exit(1);
                }

                frame = av_frame_alloc();
                if (!frame) {
                    fprintf(stderr, "Could not allocate video frame\n");
                    exit(1);
                }

                frame->format = c->pix_fmt;
                frame->width  = c->width;
                frame->height = c->height;

                ret = av_frame_get_buffer(frame, 0);
                if (ret < 0) {
                    fprintf(stderr, "Could not allocate the video frame data\n");
                    exit(1);
                }

                /* encode 1 second of video */
                for (i = 0; i < 25; i++) {
                    fflush(stdout);

        /* Make sure the frame data is writable.
           On the first round, the frame is fresh from av_frame_get_buffer()
           and therefore we know it is writable.
           But on the next rounds, encode() will have called
           avcodec_send_frame(), and the codec may have kept a reference to
           the frame in its internal structures, that makes the frame
           unwritable.
           av_frame_make_writable() checks that and allocates a new buffer
           for the frame only if necessary.
         */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        /* Prepare a dummy image.
           In real code, this is where you would have your own logic for
           filling the frame. FFmpeg does not care what you put in the
           frame.
         */
        /* Y */
        for (y = 0; y < c->height; y++) {
            for (x = 0; x < c->width; x++) {
                //frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
                frame->data[0][y * frame->linesize[0] + x] = i * 3;
            }
        }

        /* Cb and Cr */
        for (y = 0; y < c->height/2; y++) {
            for (x = 0; x < c->width/2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        /* encode the image */
        encode(c, frame, pkt, f);
    }

                /* flush the encoder */
                encode(c, NULL, pkt, f);

    /* Add sequence end code to have a real MPEG file.
       It makes only sense because this tiny examples writes packets
       directly. This is called "elementary stream" and only works for some
       codecs. To create a valid file, you usually need to write packets
       into a proper file format or protocol; see mux.c.
     */
                if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_H264)
                    fwrite(endcode, 1, sizeof(endcode), f);
                fclose(f);

                avcodec_free_context(&c);
                av_frame_free(&frame);
                av_packet_free(&pkt);
                break;
            case 'h':
                printf("hint: input para format should be like: \n <d/e> <I/O file name> <O filename/codec name>\n");
                break;
            case 'v':
                printf("ver: 1.0.0. Basic flow construction\n");
                break;
            default:
                fprintf(stderr, "wrong para input\n");
                exit(0);
                break;
        }
        
    }
    return 0;
}















