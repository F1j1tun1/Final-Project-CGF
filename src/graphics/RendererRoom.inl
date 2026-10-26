    void roomContactShadow(glm::vec3 center, glm::vec2 radius, float strength=.24f, float yaw=0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (int layer=0;layer<3;++layer) {
            const float spread=1.0f+static_cast<float>(2-layer)*.15f;
            draw(cylinder,transform({center.x,-.773f+layer*.0003f,center.z},
                 {radius.x*spread,.001f,radius.y*spread},yaw),{.008f,.004f,.012f},
                 0,glm::vec2(1),0,8,0,strength*.34f);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void roomMainLegs() {
        const glm::vec3 wood{.115f,.040f,.055f},brass{.38f,.26f,.12f};
        for (const float side : {-1.0f,1.0f})
            for (const float front : {-1.0f,1.0f}) {
                const glm::vec3 foot(side*2.88f,-.75f,front*1.47f);
                roomContactShadow(foot,{.18f,.16f},.23f);
                beam(foot,{side*2.73f,-.23f,front*1.35f},.095f,wood);
                draw(cylinder,transform(foot,{.13f,.04f,.13f}),brass,0,glm::vec2(1),.18f,40);
                draw(cylinder,transform({side*2.76f,-.32f,front*1.38f},{.105f,.045f,.105f}),
                     brass,0,glm::vec2(1),.18f,40);
            }
    }

    void roomCurtainRail(glm::vec3 position, glm::vec3 scale, float yaw, const glm::mat4& parent=glm::mat4(1)) {
        const glm::mat4 root=parent*transform(position,scale,yaw);
        const glm::vec3 brass{.28f,.19f,.10f};
        constexpr float top=4.405f;
        beam({-1.82f,top,.04f},{1.82f,top,.04f},.040f,brass,root);
        for (const float side : {-1.0f,1.0f})
            draw(sphere,root*transform({side*1.84f,top,.04f},{.068f,.068f,.068f}),
                 brass,0,glm::vec2(1),.18f,40);
        for (int i=0;i<9;++i) {
            const float x=-1.54f+static_cast<float>(i)*.385f;
            glm::mat4 model=glm::translate(root,{x,top,.04f});
            model=glm::rotate(model,pi*.5f,glm::vec3(0,0,1));
            draw(ring,glm::scale(model,{.080f,.025f,.10f}),brass,
                 0,glm::vec2(1),.18f,40);
        }
    }

    void drawRoom() {
        // Auditorium is 46.4 wide, 75 deep and 60 high. Stage geometry is
        // enlarged in all axes and translated deeper; no camera-dependent scaling.
        const glm::vec3 bronze{.46f,.32f,.16f}, plaster{.29f,.255f,.205f};
        const glm::vec3 wood{.105f,.078f,.060f}, stone{.39f,.33f,.24f};
        const glm::mat4 shellRoot=glm::translate(glm::mat4(1),{0,.234f,0})*glm::scale(glm::mat4(1),{1.45f,1.3f,1});
        const auto masonry=[&](glm::vec3 pos,glm::vec3 size,glm::vec3 color) {
            draw(cube,shellRoot*transform(pos,size),color,0,glm::vec2(1),.025f,20,0,1,0,11);
        };
        draw(cube,shellRoot*transform({0,-.92f,-13.5f},{32.6f,.28f,75.6f}),{.38f,.27f,.17f},
             0,glm::vec2(1),.10f,28,0,1,0,12);
        // A continuous shell remains present at every supported orbit angle.
        masonry({0,22.6f,-51},{32.6f,46.8f,.5f},plaster*.62f);
        masonry({0,22.6f,24},{32.6f,46.8f,.5f},plaster*.62f);
        masonry({0,46,-13.5f},{32.6f,.5f,75.6f},plaster*.75f);
        for(float side : {-1.0f,1.0f}) {
            masonry({side*16,22.6f,-13.5f},{.5f,46.8f,75.6f},plaster*.77f);
            masonry({side*15.65f,44.7f,-13.5f},{.55f,.6f,75},stone*.72f);
            masonry({side*15.72f,.10f,-13.5f},{.4f,1.8f,75},wood);
            for(float z : {-30.0f,-20.0f,-10.0f,0.0f,10.0f,20.0f}) {
                masonry({side*15.5f,21.6f,z},{.65f,44.8f,.85f},stone*.75f);
                masonry({side*15.35f,43.6f,z},{.95f,.4f,1.3f},stone);
            }
            // Deep side boxes, with slabs, load-bearing brackets and balustrades.
            for(float z : {-10.0f,3.0f,16.0f}) {
                masonry({side*14.25f,4.1f,z},{3.5f,.48f,7.4f},stone*.8f);
                masonry({side*12.48f,4.04f,z},{.24f,.62f,7.5f},stone*.70f);
                masonry({side*12.43f,3.73f,z},{.30f,.10f,7.6f},bronze*.45f);
                masonry({side*12.60f,4.75f,z},{.22f,.95f,7.4f},wood);
                draw(cube,shellRoot*transform({side*12.58f,5.25f,z},{.3f,.10f,7.5f}),bronze);
                for(float end : {-1.0f,1.0f}) {
                    masonry({side*14.25f,4.75f,z+end*3.6f},{3.5f,.95f,.22f},wood);
                    // Stepped masonry corbels grow out of the box, with no exposed tube ends.
                    for(int tier=0;tier<3;++tier)
                        masonry({side*(15.05f-tier*.38f),3.1f+tier*.30f,z+end*2.6f},
                                 {1.8f+tier*.72f,.34f,.65f},stone*.60f);
                }
                for(int i=0;i<9;++i)
                    draw(cube,shellRoot*transform({side*12.45f,4.75f,z-3.2f+i*.8f},{.04f,.75f,.05f}),bronze*.65f);
            }
        }
        // Three broad ceiling ribs, no small coffers or camera-dependent cutaway.
        for(float z : {-15.0f,1.0f,17.0f})
            masonry({0,45.55f,z},{32,.65f,.7f},stone*.65f);

        {
        // Stage: 26.1 x 13.75, apron z=-35.5; all supports and lights share this root.
        const glm::mat4 stageRoot=glm::translate(glm::mat4(1),{0,.234f,-8})
            *glm::scale(glm::mat4(1),{1.45f,1.3f,1.25f});
        const auto stageMasonry=[&](glm::vec3 pos,glm::vec3 size,glm::vec3 color) {
            draw(cube,stageRoot*transform(pos,size),color,0,glm::vec2(1),.025f,20,0,1,0,11);
        };
        // Raised stage and three real, walkable-depth stair treads.
        draw(cube,stageRoot*transform({0,-.08f,-27.5f},{18,1.30f,11}),wood,
             0,glm::vec2(1),.08f,24,0,1,0,12);
        for(int step=0;step<3;++step) {
            const float top=-.39f+step*.32f;
            draw(cube,stageRoot*transform({0,(top-.78f)*.5f,-20.4f-step*.55f},
                 {6.8f-step*.2f,top+.78f,1.1f}),stone*.68f);
            draw(cube,stageRoot*transform({0,top+.015f,-20.14f-step*.55f},{6.8f-step*.2f,.025f,.035f}),bronze);
        }
        // Solid proscenium piers and an extruded elliptical arch, not a backdrop.
        for(float side : {-1.0f,1.0f}) {
            stageMasonry({side*12.5f,9.6f,-24.3f},{7,20.8f,1.1f},plaster);
            stageMasonry({side*9.1f,2.65f,-23.55f},{1.0f,6.9f,1.25f},stone);
            stageMasonry({side*9.1f,-.35f,-23.4f},{1.5f,.85f,1.5f},stone*.85f);
            stageMasonry({side*9.1f,5.85f,-23.4f},{1.5f,.38f,1.5f},stone);
            draw(cube,stageRoot*transform({side*8.75f,2.8f,-22.88f},{.07f,6,.05f}),bronze);
        }
        stageMasonry({0,15.1f,-24.3f},{18,10,1.1f},plaster);
        for(int segment=0;segment<40;++segment) {
            const float a=(segment+.5f)*pi/40;
            const float x=9.1f*std::cos(a),y=6+4.1f*std::sin(a);
            const float tangent=std::atan2(4.1f*std::cos(a),-9.1f*std::sin(a));
            const float length=glm::length(glm::vec2(9.1f*std::sin(a),4.1f*std::cos(a)))*pi/40+.025f;
            auto model=glm::translate(stageRoot,{x,y,-23.55f});
            model=glm::rotate(model,tangent,glm::vec3(0,0,1));
            draw(cube,glm::scale(model,{length,.70f,1.3f}),stone,0,glm::vec2(1),.04f,24,0,1,0,11);
        }
        // Masonry spandrels close the space above the curved opening.
        for(int strip=0;strip<40;++strip) {
            const float x=-8.8f+(strip+.5f)*17.6f/40;
            const float bottom=6+4.1f*std::sqrt(std::max(0.0f,1-x*x/(9.1f*9.1f)));
            stageMasonry({x,(bottom+10.35f)*.5f,-24.3f},{.45f,10.35f-bottom,1.1f},plaster);
        }
        draw(arch,stageRoot*transform({0,6,-22.88f},{8.85f,3.95f,1}),bronze,0,glm::vec2(1),.15f,32,.05f);
        // Wing curtains hang from a truss inside the stage, with visible hangers.
        beam({-9,9.0f,-25},{9,9.0f,-25},.12f,wood,stageRoot);
        for(float side : {-1.0f,1.0f}) {
            beam({side*8.8f,9,-25},{side*9.5f,9,-24.3f},.10f,bronze,stageRoot);
            const glm::vec3 pos{side*6.7f,1.80f,-25};
            const glm::vec3 scale{1.15f,1.62f,1};
            draw(curtain,stageRoot*transform(pos,scale),{.19f,.067f,.052f},felt,{7,8},.02f,14,0,1,.7f,2);
            roomCurtainRail(pos,scale,0,stageRoot);
        }
        // Rear drape is eleven units behind the apron; side returns close the wings.
        draw(curtain,stageRoot*transform({0,1.80f,-32.8f},{5.1f,1.62f,1}),{.255f,.205f,.14f},
             felt,{14,8},.02f,14,0,1,.65f,2);
        roomCurtainRail({0,1.80f,-32.8f},{5.1f,1.62f,1},0,stageRoot);
        for(float x : {-8.5f,0.0f,8.5f})
            beam({x,8.94f,-33.75f},{x,8.94f,-32.76f},.075f,bronze,stageRoot);
        for(float side : {-1.0f,1.0f})
            stageMasonry({side*9.25f,4.6f,-29},{.5f,8.3f,9.5f},wood);
        // Stage luminaires attach to the same overhead truss; shader lights match.
        for(float side : {-1.0f,1.0f}) {
            beam({side*6,9,-25},{side*6,8.6f,-25},.05f,bronze,stageRoot);
            draw(sphere,stageRoot*transform({side*6,8.5f,-25},{.25f,.20f,.30f}),wood);
            draw(sphere,stageRoot*transform({side*6,8.35f,-25.18f},{.16f,.08f,.16f}),{1,.73f,.35f},
                 0,glm::vec2(1),0,8,1.4f);
        }
        for(int i=0;i<9;++i) {
            const float x=-7.2f+i*1.8f;
            draw(cube,stageRoot*transform({x,.65f,-22.18f},{.30f,.16f,.22f}),wood);
            draw(sphere,stageRoot*transform({x,.74f,-22.16f},{.10f,.05f,.07f}),{1,.68f,.28f},
                 0,glm::vec2(1),0,8,1.2f);
        }
        }
        // One physical pendant: the cable reaches the structural ceiling at y=60.
        // Its far attachment lies above every permitted gameplay view, not behind a cutaway.
        const glm::vec3 lamp{0,5.4f,-1.6f};
        beam({0,5.90f,-1.6f},{0,60.0f,-1.6f},.025f,{.095f,.078f,.060f});
        draw(cylinder,transform({0,59.94f,-1.6f},{.28f,.12f,.28f}),wood);
        draw(cylinder,transform(lamp+glm::vec3(0,.44f,0),{.12f,.30f,.12f}),bronze*.65f);
        draw(sphere,transform(lamp+glm::vec3(0,.32f,0),{.66f,.28f,.66f}),{.13f,.105f,.065f},
             0,glm::vec2(1),.22f,40);
        draw(cylinder,transform(lamp+glm::vec3(0,.03f,0),{.66f,.06f,.66f}),bronze*.70f);
        draw(cylinder,transform(lamp,{.56f,.025f,.56f}),{1,.83f,.54f},0,glm::vec2(1),0,8,1.2f);
        roomMainLegs();
    }

    void drawRoomDust(float time, bool animate) {
        const float t=animate?time:0;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        // Fixed world-space populations behind the card area and inside the stage lights.
        // No spawning/wrapping discontinuities: slow bounded drift, 48 grains total.
        for(int i=0;i<48;++i) {
            const float phase=i*2.399963f;
            const bool stage=i>=24;
            const float x=std::sin(phase)*(stage?9.0f:2.8f)+std::sin(t*.11f+phase)*.18f;
            const float y=(stage?2.0f:1.4f)+(i%7)*(stage?.75f:.35f)+std::sin(t*.09f+phase)*.16f;
            const float z=(stage?-42.0f:-4.4f)+std::cos(phase*1.7f)*(stage?3.4f:.55f)
                +std::sin(t*.07f+phase)*.12f;
            const float radius=(stage?.025f:.010f)+(i%3)*(stage?.009f:.004f);
            const float opacity=.11f+(i%4)*.025f;
            draw(sphere,transform({x,y,z},{radius,radius,radius}),{.72f,.60f,.39f},
                 0,glm::vec2(1),0,8,.25f,opacity);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
