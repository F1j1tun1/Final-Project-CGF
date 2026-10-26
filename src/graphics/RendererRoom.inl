    float roomLampBrightness(float time, bool animate) const {
        if (!animate) return 1.0f;
        const float phase=std::fmod(time,31.0f);
        const float first=(phase-12.0f)/.55f;
        const float second=(phase-13.3f)/.45f;
        return 1.0f-.16f*std::exp(-first*first)-.07f*std::exp(-second*second);
    }

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

    void roomLamp(glm::vec3 bulb, float brightness) {
        const glm::vec3 brass{.28f,.20f,.105f};
        beam(bulb+glm::vec3(0,.35f,0),{bulb.x,4.1f,bulb.z},.009f,{.045f,.034f,.032f});
        draw(cylinder,transform({bulb.x,4.10f,bulb.z},{.12f,.035f,.12f}),brass);
        draw(sphere,transform(bulb+glm::vec3(0,.25f,0),{.46f,.30f,.46f}),
             {.43f,.33f,.22f},0,glm::vec2(1),.035f,18,.24f*brightness);
        draw(cylinder,transform(bulb+glm::vec3(0,.05f,0),{.42f,.04f,.42f}),brass,
             0,glm::vec2(1),.13f,40);
        draw(cylinder,transform(bulb+glm::vec3(0,.022f,0),{.37f,.012f,.37f}),
             {.85f,.55f,.25f},0,glm::vec2(1),0,8,.85f*brightness);
        draw(sphere,transform(bulb,{.065f,.065f,.065f}),{1,.76f,.40f},
             0,glm::vec2(1),0,8,1.20f*brightness);
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

    void roomColumn(glm::vec3 position, float top, float shade=1) {
        const glm::vec3 stone=glm::vec3(.18f,.125f,.185f)*shade;
        const glm::vec3 brass=glm::vec3(.48f,.32f,.16f)*shade;
        const float height=top+.775f;
        draw(cylinder,transform({position.x,(top-.775f)*.5f,position.z},{.17f,height,.17f}),
             stone,0,glm::vec2(1),.08f,28,.015f);
        draw(cylinder,transform({position.x,-.68f,position.z},{.28f,.18f,.28f}),
             brass,0,glm::vec2(1),.20f,48);
        draw(cylinder,transform({position.x,top-.04f,position.z},{.27f,.13f,.27f}),
             brass,0,glm::vec2(1),.20f,48);
    }

    void roomPortal(float z, float spring, float radiusX, float radiusY, float shade) {
        const glm::vec3 stone=glm::vec3(.20f,.14f,.205f)*shade;
        const glm::vec3 brass=glm::vec3(.50f,.34f,.17f)*shade;
        draw(arch,transform({0,spring,z},{radiusX,radiusY,1}),stone,
             0,glm::vec2(1),.10f,36,.025f);
        draw(arch,transform({0,spring,z+.025f},{radiusX-.12f,radiusY-.11f,1}),brass,
             0,glm::vec2(1),.22f,56,.02f);
        for (const float side : {-1.0f,1.0f})
            roomColumn({side*(radiusX-.14f),0,z+.035f},spring,shade);
        diamond({0,spring+radiusY-.06f,z+.055f},.19f,brass);
    }

    void roomCurtainRail(glm::vec3 position, glm::vec3 scale, float yaw) {
        const glm::mat4 root=transform(position,scale,yaw);
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

    void drawRoom(float time, bool animate) {
        const glm::vec3 bronze{.43f,.29f,.145f};
        draw(cube,transform({0,-.92f,-10},{50,.25f,64}),{.042f,.030f,.048f});
        draw(cube,transform({0,-.79f,-10},{50,.03f,64}),{.12f,.10f,.12f},
             0,glm::vec2(1),.18f,48,0,1,0,3);

        // An open proscenium frames the host; successive portals give it depth.
        roomPortal(-6.50f,1.12f,2.50f,1.62f,1.0f);
        roomPortal(-10.80f,1.02f,2.20f,1.28f,.64f);
        roomPortal(-16.00f,.82f,1.90f,1.00f,.38f);

        // Mirrored wings share level hems, rails and matching tiebacks.
        for (const float side : {-1.0f,1.0f}) {
            const glm::vec3 inner{side*3.90f,-.04f,-6.425f};
            const glm::vec3 outer{side*7.25f,-.04f,-7.125f};
            const glm::vec3 innerScale{-side*.77f,.98f,1};
            const glm::vec3 outerScale{-side*.68f,.98f,1};
            draw(curtain,transform(inner,innerScale),{.245f,.029f,.080f},
                 felt,{7,8},.035f,16,.025f,1,.95f,2);
            draw(curtain,transform(outer,outerScale),{.130f,.036f,.20f},
                 felt,{7,8},.03f,16,.015f,1,.9f,2);
            roomCurtainRail(inner,innerScale,0);
            roomCurtainRail(outer,outerScale,0);
            draw(cube,transform({side*3.90f,.98f,-6.065f},{1.60f,.055f,.055f}),bronze*.80f);
            diamond({side*3.90f,.81f,-6.015f},.15f,bronze*.95f);

            roomColumn({side*5.55f,0,-6.95f},2.58f);
            roomColumn({side*5.75f,0,-11.35f},2.48f,.64f);
            roomColumn({side*6.15f,0,-16.5f},2.32f,.40f);
            draw(cube,transform({side*2.20f,-.772f,-13.0f},{.028f,.003f,18}),
                 bronze*.24f,0,glm::vec2(1),.06f,24);
        }

        const float lampBrightness=roomLampBrightness(time,animate);
        for (const float side : {-1.0f,1.0f}) {
            roomLamp({side*4.7f,3.1f,-5.75f},lampBrightness);
            const float x=side*5.45f;
            draw(cylinder,transform({x,-.64f,-2.6f},{.27f,.14f,.27f}),bronze,0,glm::vec2(1),.26f,48);
            beam({x,-.61f,-2.6f},{x,1.03f,-2.6f},.048f,bronze);
            for (int branch=-1;branch<=1;++branch) {
                const float bx=x+branch*.25f,by=branch==0?1.18f:.96f;
                beam({x,.70f,-2.6f},{bx,by,-2.6f},.030f,bronze);
                draw(cylinder,transform({bx,by,-2.6f},{.11f,.05f,.11f}),bronze);
                draw(cylinder,transform({bx,by+.23f,-2.6f},{.063f,.42f,.063f}),{.73f,.62f,.41f});
                const float drift=animate?std::sin(time*1.3f+bx+branch*.8f)*.006f:0;
                draw(sphere,transform({bx+drift*.3f,by+.51f,-2.6f},{.040f,.10f+drift,.040f}),
                     {1,.44f,.10f},0,glm::vec2(1),0,8,1.40f);
                draw(sphere,transform({bx,by+.48f,-2.56f},{.021f,.050f,.021f}),
                     {1,.82f,.43f},0,glm::vec2(1),0,8,1.70f);
            }
        }
        roomMainLegs();
    }
