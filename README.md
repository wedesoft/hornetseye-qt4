hornetseye-qt4
==============

**Author**:       Jan Wedekind
**Copyright**:    2010
**License**:      GPL

Synopsis
--------

This Ruby extension implements a Qt4 widget (compatible with *qtruby4*) for hardware accelerated video output.

Installation
------------

This Ruby extension requires Qt4, QtRuby4 and the software scaling library. If you are running Debian or (K)ubuntu, you can install them like this:

    $ sudo aptitude install libqt4-ruby1.8 qt4-dev-tools libswscale-dev

To install this Ruby extension, use the following command:

    $ sudo gem install hornetseye-qt4

Alternatively you can build and install the Ruby extension from source as follows:

    $ rake
    $ sudo rake install

Usage
-----

Simply run Interactive Ruby:

    $ irb

You can use {Hornetseye::XvWidget} as shown below. This example implements a video player with a minimalistic GUI.

    require 'rubygems'
    require 'hornetseye_ffmpeg'
    require 'hornetseye_alsa'
    require 'hornetseye_qt4'
    require 'Qt4'
    VIDEO = ARGV.first
    class Win < Qt::Widget
      slots 'seek(int)'
      def initialize
        super
        @screen = Hornetseye::XvWidget.new self
        @slider = Qt::Slider.new Qt::Horizontal
        layout = Qt::VBoxLayout.new self
        layout.addWidget @screen
        layout.addWidget @slider
        connect @slider, SIGNAL('valueChanged(int)'), self, SLOT('seek(int)')
        @seeking = true
        @video = nil
        @timer = 0
        start
        @slider.tracking = false
        @slider.minimum = 0
        @slider.single_step = 60
        @slider.page_step = 600
        if @video.duration
          @slider.maximum = @video.duration.to_i
        else
          @slider.maximum = ARGV[1] || 1
        end
        setWindowTitle 'XVideo'
      end
      def update_audio
        @audio_frame = @video.read_audio unless @audio_frame
        while @speaker.avail >= @audio_frame.shape[1]
          @speaker.write @audio_frame
          @audio_frame = @video.read_audio
        end
      end
      def update_video
        @screen.write @video.read_video
      end
      def timerEvent( e )
        begin
          update_audio
          update_video
          unless @slider.sliderDown
            @seeking = false
            p = @video.audio_pos.to_i
            @slider.maximum = p if p > @slider.maximum
            @slider.value = p
            @seeking = true
          end
          t = @video.audio_pos - @speaker.delay.quo( @speaker.rate )
          delay = [ 3.quo( 2 ) / @video.frame_rate,
                    [ @video.video_pos - t, 0 ].max ].min
          killTimer @timer
          @timer = startTimer( ( delay * 1000 ).to_i )
        rescue Exception => e
          p e
          stop
        end
      end
      def start
        unless @video
          stop
          @video = Hornetseye::AVInput.new VIDEO
          @audio_frame = nil
          resize ( @video.width * @video.aspect_ratio ).to_i, @video.height
          @speaker = Hornetseye::AlsaOutput.new 'default:0',
                                                @video.sample_rate, @video.channels
          @timer = startTimer 0
        end
      end
      def stop
        @audio_frame = nil
        if @video
          @video.close
          @video = nil
        end
        if @speaker
          @speaker.close
          @speaker = nil
        end
        if @timer != 0
          killTimer @timer
          @timer = 0
          @screen.clear
        end
      end
      def seek( p )
        if @seeking
          begin
            start
            @audio_frame = nil
            @video.pos = p
            @speaker.drop
            @speaker.prepare
          rescue Exception => e
            p e
            stop
          end
        end
      end
    end
    app = Qt::Application.new ARGV
    Win.new.show
    app.exec
 
