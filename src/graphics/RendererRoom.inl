    void roomContactShadow(glm::vec3 center, glm::vec2 radius, float strength=.24f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (int layer=0; layer<3; ++layer) {
            const float spread=1.0f+static_cast<float>(2-layer)*.15f;
            draw(cylinder, transform({center.x,-.773f+layer*.0003f,center.z},
                 {radius.x*spread,.001f,radius.y*spread}), {.008f,.004f,.012f},
                 0,glm::vec2(1),0,8,0,strength*.34f);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void roomChair(glm::vec3 position, float yaw, glm::vec3 upholstery) {
        const glm::vec3 brass{.30f,.21f,.10f};
        const glm::mat4 root=transform(position,glm::vec3(1),yaw);
        roomContactShadow(position,{.34f,.32f},.17f);
        for (const float x : {-.21f,.21f})
            for (const float z : {-.18f,.18f})
                beam({x,-.75f,z},{x*.83f,-.32f,z*.83f},.024f,brass,root);
        draw(cylinder,root*transform({0,-.32f,0},{.32f,.075f,.30f}),brass,0,glm::vec2(1),.34f,64);
        draw(cylinder,root*transform({0,-.27f,0},{.30f,.095f,.28f}),upholstery*.80f,felt,{3,3},.04f,16,0,1,.7f,2);
        draw(sphere,root*transform({0,.08f,.26f},{.31f,.39f,.08f}),brass,0,glm::vec2(1),.24f,56);
        draw(sphere,root*transform({0,.08f,.275f},{.275f,.35f,.075f}),upholstery*.80f,
             felt,{3,3},.045f,16,0,1,.7f,2);
    }

    void roomLamp(glm::vec3 bulb) {
        const glm::vec3 brass{.43f,.29f,.12f};
        beam(bulb+glm::vec3(0,.35f,0),{bulb.x,4.1f,bulb.z},.012f,brass);
        draw(cylinder,transform({bulb.x,4.10f,bulb.z},{.11f,.035f,.11f}),brass);
        draw(sphere,transform(bulb+glm::vec3(0,.22f,0),{.36f,.25f,.36f}),
             {.65f,.48f,.28f},0,glm::vec2(1),.07f,24,.34f);
        draw(cylinder,transform(bulb+glm::vec3(0,.05f,0),{.33f,.035f,.33f}),brass,
             0,glm::vec2(1),.36f,72);
        draw(cylinder,transform(bulb+glm::vec3(0,.024f,0),{.29f,.012f,.29f}),
             {1,.67f,.30f},0,glm::vec2(1),0,8,.95f);
        draw(sphere,transform(bulb,{.055f,.055f,.055f}),{1,.79f,.46f},
             0,glm::vec2(1),0,8,1.35f);
    }

    void roomDealer(glm::vec3 table, float scale, float period, float phase,
                    float time, bool animate, bool reverseColors) {
        const glm::vec3 plum{.090f,.027f,.13f}, wine{.19f,.019f,.047f};
        const glm::vec3 brass{.36f,.24f,.10f}, ivory{.48f,.43f,.35f};
        const float t=animate?time:0.0f;
        const float turn=.045f*std::sin(t*(2*pi/period)+phase);
        const glm::mat4 actor=transform({table.x,-.10f,table.z-1.04f},glm::vec3(scale),turn);
        draw(jacket,actor,reverseColors?wine:plum,felt,{3,3},.065f,18,.008f,1,.8f,2);
        const float footY=-.675f/scale;
        for (const float side : {-1.0f,1.0f}) {
            beam({side*.20f,-.12f,0},{side*.24f,footY+.09f,0},.085f,plum*.50f,actor);
            draw(sphere,actor*transform({side*.24f,footY+.055f,.07f},{.12f,.055f,.19f}),{.035f,.018f,.033f});
        }
        draw(ruff,actor*transform({0,1.42f,.01f},{.88f,.85f,.88f}),ivory*.69f,
             felt,{4,2},.06f,20,.02f,1,.5f,2);
        glm::mat4 head=actor*glm::translate(glm::mat4(1),{0,1.94f,-.025f});
        head=glm::rotate(head,-.10f+.025f*std::sin(t*(2*pi/(period+1.2f))+phase),glm::vec3(1,0,0));
        draw(hood,head*glm::translate(glm::mat4(1),{0,0,-.16f}),plum*.35f,
             felt,{3,3},.04f,16,0,1,.5f,2);
        draw(mask,head,ivory,0,glm::vec2(1),.16f,56,.010f,1,0,1);
        for (const float side : {-1.0f,1.0f}) {
            const glm::mat4 hat=head*transform({side*.11f,.39f,-.035f},glm::vec3(.73f),side<0?pi:0);
            draw(horn,hat,(side<0)!=reverseColors?plum:wine,felt,{3,3},.06f,20,.008f,1,.65f,2);
            const glm::vec3 bell=glm::vec3(hat*glm::vec4(.92f,.72f*std::sin(pi*.88f),0,1));
            draw(sphere,transform(bell,glm::vec3(.029f)),brass,0,glm::vec2(1),.40f,72,.025f);
        }
        for (int button=0;button<3;++button)
            diamond({0,.47f+button*.25f,.30f},.035f,brass,actor);
        for (const float side : {-1.0f,1.0f}) {
            const float wave=std::max(0.0f,std::sin(t*(2*pi/period)+phase+(side<0?1.8f:0.0f)));
            const float reach=wave*wave*wave;
            const glm::vec3 shoulder(side*.64f,1.21f,0);
            const glm::vec3 elbow=glm::mix(glm::vec3(side*.89f,.71f,.12f),
                                          glm::vec3(side*.78f,.91f,.34f),reach);
            const glm::vec3 wrist=glm::mix(glm::vec3(side*.96f,.22f,.44f),
                                          glm::vec3(side*.54f,.48f,.95f),reach);
            const auto cloth=(side<0)!=reverseColors?plum:wine;
            beam(shoulder,elbow,.145f,cloth,actor);
            draw(sphere,actor*transform(elbow,glm::vec3(.14f)),cloth);
            beam(elbow,wrist,.092f,cloth,actor);
            draw(cylinder,actor*transform(wrist,{.11f,.09f,.11f}),ivory*.70f);
            draw(glove,actor*transform(wrist+glm::vec3(0,-.025f,.025f),{side*.60f,.60f,.60f}),
                 ivory,0,glm::vec2(1),.05f,20,.005f);
        }
    }

    void roomBackgroundTable(glm::vec3 center, int index, float time, bool animate) {
        const glm::vec3 brass{.34f,.22f,.095f}, wood{.085f,.028f,.045f};
        const glm::mat4 root=glm::translate(glm::mat4(1),center);
        roomContactShadow(center,{1.28f,.88f},.19f);
        for (const float side : {-1.0f,1.0f})
            for (const float front : {-1.0f,1.0f}) {
                beam({side*.73f,-.74f,front*.40f},{side*.68f,-.18f,front*.36f},.052f,wood,root);
                draw(cylinder,root*transform({side*.73f,-.75f,front*.40f},{.08f,.04f,.08f}),brass);
            }
        draw(cylinder,root*transform({0,-.13f,0},{1.25f,.22f,.87f}),wood,
             0,glm::vec2(1),.12f,40);
        draw(cylinder,root*transform({0,-.018f,0},{1.26f,.038f,.88f}),brass,
             0,glm::vec2(1),.38f,64);
        draw(cylinder,root*transform({0,.006f,0},{1.18f,.025f,.80f}),{.024f,.17f,.095f},
             felt,{7,5},.025f,12,.02f,1,.65f);
        draw(cube,root*transform({0,.021f,.12f},{.92f,.004f,.009f}),brass*.65f);
        draw(cylinder,root*transform({-.67f,.042f,-.13f},{.075f,.025f,.075f}),
             index%2==0?glm::vec3(.43f,.085f,.10f):glm::vec3(.26f,.10f,.38f),chip,glm::vec2(1),.14f,32);
        draw(cylinder,root*transform({-.51f,.033f,-.04f},{.070f,.020f,.070f}),brass,
             chip,glm::vec2(1),.14f,32);
        const glm::mat4 card=root*transform({.23f,.028f,.14f},{.22f,.010f,.33f},-.18f+index*.11f);
        draw(cube,card,{.75f,.69f,.56f});
        draw(plane,card,glm::vec3(.90f),cardBack());
        const glm::vec3 upholstery=index%2==0?glm::vec3(.18f,.035f,.075f):glm::vec3(.115f,.045f,.17f);
        roomChair(center+glm::vec3(-.59f,0,1.12f),.10f,upholstery);
        roomChair(center+glm::vec3(.49f,0,1.03f),-.16f,upholstery);
        const float outer=center.x<0?-1.0f:1.0f;
        roomChair(center+glm::vec3(outer*1.43f,0,.06f),outer*pi*.5f,upholstery);
        constexpr float scales[]={.64f,.62f,.57f,.55f};
        constexpr float periods[]={4.8f,5.9f,6.8f,7.7f};
        roomDealer(center,scales[index],periods[index],index*.87f,time,animate,index%2!=0);
        roomLamp(center+glm::vec3(outer*.65f,1.96f,-.15f));
    }

    void roomMainLegsAndProps() {
        const glm::vec3 wood{.115f,.040f,.055f},brass{.46f,.30f,.12f};
        for (const float side : {-1.0f,1.0f})
            for (const float front : {-1.0f,1.0f}) {
                const glm::vec3 foot(side*2.88f,-.75f,front*1.47f);
                roomContactShadow(foot,{.18f,.16f},.27f);
                beam(foot,{side*2.73f,-.23f,front*1.35f},.095f,wood);
                draw(cylinder,transform(foot,{.13f,.04f,.13f}),brass,0,glm::vec2(1),.32f,56);
                draw(cylinder,transform({side*2.76f,-.32f,front*1.38f},{.105f,.045f,.105f}),brass,
                     0,glm::vec2(1),.36f,64);
            }
        // A single dropped card, a few chips and one small bell, never mirrored.
        const glm::vec3 paper{-3.64f,-.770f,1.57f};
        roomContactShadow(paper,{.17f,.23f},.21f);
        const glm::mat4 card=transform(paper,{.27f,.009f,.39f},.31f);
        draw(cube,card,{.78f,.72f,.59f},0,glm::vec2(1),.025f);
        draw(plane,card,glm::vec3(.93f),cardFaces[28],glm::vec2(1),.025f);
        const std::array<glm::vec3,3> chips{{{-3.88f,-.762f,1.27f},
                                           {-3.79f,-.762f,1.44f},{3.73f,-.762f,1.51f}}};
        for (std::size_t i=0;i<chips.size();++i) {
            roomContactShadow(chips[i],{.077f,.077f},.25f);
            draw(cylinder,transform(chips[i],{.065f,.020f,.065f},i*.73f),
                 i==1?glm::vec3(.34f,.12f,.39f):glm::vec3(.50f,.12f,.12f),chip,
                 glm::vec2(1),.20f,48);
        }
        const glm::vec3 bell{3.88f,-.711f,1.34f};
        roomContactShadow(bell,{.075f,.072f},.24f);
        draw(sphere,transform(bell,{.060f,.060f,.060f}),brass,0,glm::vec2(1),.58f,96);
        draw(cube,transform(bell+glm::vec3(0,-.008f,.058f),{.073f,.004f,.006f},.21f),{.035f,.020f,.025f});
    }

    void drawRoom(float time, bool animate) {
        const glm::vec3 bronze{.45f,.30f,.13f},stone{.115f,.080f,.12f};
        draw(cube,transform({0,-.92f,-8},{32,.25f,42}),{.045f,.030f,.045f});
        draw(cube,transform({0,-.79f,-10},{50,.03f,64}),{.12f,.10f,.12f},
             0,glm::vec2(1),.23f,64,0,1,0,3);
        draw(cube,transform({0,3,-22},{30,8,.28f}),{.055f,.025f,.065f});
        // Tall side arcades frame an open room, rather than hiding the tables.
        for (const float side : {-1.0f,1.0f}) {
            draw(cube,transform({side*10.5f,1.35f,-12},{.24f,4.3f,23}),{.070f,.030f,.065f});
            for (int bay=0;bay<4;++bay) {
                const float z=-5.5f-bay*4.4f;
                const float x=side*(bay==0?7.8f:8.6f);
                draw(cylinder,transform({x,1.04f,z},{.19f,3.58f,.19f}),stone);
                draw(cylinder,transform({x,-.67f,z},{.29f,.18f,.29f}),bronze*.68f);
                draw(cylinder,transform({x,2.80f,z},{.28f,.14f,.28f}),bronze*.72f);
                draw(arch,transform({x-side*1.74f,2.76f,z},{1.75f,1.30f,1}),stone);
                draw(cube,transform({side*10.34f,1.08f,z-1.95f},{.04f,2.55f,2.35f}),
                     bay%2==0?glm::vec3(.125f,.030f,.075f):glm::vec3(.095f,.035f,.14f),
                     felt,{4,5},.04f,16,.015f,1,.75f,2);
                draw(cube,transform({side*10.28f,2.44f,z-1.95f},{.05f,.035f,2.55f}),bronze*.62f);
            }
            draw(curtain,transform({side*9.05f,0,-5.6f},{.43f,.91f,1}),{.19f,.025f,.070f},
                 felt,{5,7},.045f,16,0,1,.85f,2);
            draw(cube,transform({side*9.05f,.93f,-5.25f},{1.13f,.045f,.04f}),bronze*.70f);
        }
        for (int panel=-3;panel<=3;++panel) {
            const float x=panel*3.3f;
            draw(cube,transform({x,1.16f,-21.8f},{2.75f,2.7f,.055f}),
                 panel%2==0?glm::vec3(.125f,.040f,.11f):glm::vec3(.075f,.035f,.115f),
                 felt,{4,4},.035f,14,.035f,1,.65f,2);
            draw(cube,transform({x,2.57f,-21.72f},{2.9f,.045f,.035f}),bronze*.68f);
            diamond({x,1.67f,-21.70f},.22f,bronze*.75f);
        }
        const glm::vec3 tables[]={{-4.35f,0,-5.10f},{4.65f,0,-6.00f},
                                  {-2.90f,0,-8.05f},{3.05f,0,-8.90f}};
        for (int index=0;index<4;++index) roomBackgroundTable(tables[index],index,time,animate);

        for (const float side : {-1.0f,1.0f}) {
            const float x=side*5.45f;
            draw(cylinder,transform({x,-.64f,-2.6f},{.27f,.14f,.27f}),bronze,0,glm::vec2(1),.45f,56);
            beam({x,-.61f,-2.6f},{x,1.03f,-2.6f},.048f,bronze);
            for (int branch=-1;branch<=1;++branch) {
                const float bx=x+branch*.25f,by=branch==0?1.18f:.96f;
                beam({x,.70f,-2.6f},{bx,by,-2.6f},.030f,bronze);
                draw(cylinder,transform({bx,by,-2.6f},{.11f,.05f,.11f}),bronze);
                draw(cylinder,transform({bx,by+.23f,-2.6f},{.063f,.42f,.063f}),{.76f,.64f,.41f});
                const float flicker=animate?std::sin(time*7.3f+bx*8)*.017f:0;
                draw(sphere,transform({bx+flicker*.3f,by+.51f,-2.6f},{.040f,.10f+flicker,.040f}),
                     {1,.44f,.10f},0,glm::vec2(1),0,8,1.5f);
            }
        }
        roomMainLegsAndProps();
    }
