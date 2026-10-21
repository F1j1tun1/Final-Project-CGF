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

    void roomChair(glm::vec3 position, float yaw, glm::vec3 upholstery) {
        const glm::vec3 brass{.25f,.18f,.095f};
        const glm::mat4 root=transform(position,glm::vec3(1),yaw);
        roomContactShadow(position,{.47f,.43f},.17f,yaw);
        for (const float x : {-.31f,.31f})
            for (const float z : {-.26f,.26f})
                beam({x,-.75f,z},{x*.86f,-.30f,z*.86f},.034f,brass,root);
        draw(cylinder,root*transform({0,-.30f,0},{.46f,.09f,.43f}),brass,0,glm::vec2(1),.12f,40);
        draw(cylinder,root*transform({0,-.24f,0},{.43f,.11f,.40f}),upholstery,
             felt,{4,4},.025f,14,0,1,.9f,2);
        draw(sphere,root*transform({0,.21f,.36f},{.44f,.51f,.11f}),brass,0,glm::vec2(1),.10f,36);
        draw(sphere,root*transform({0,.21f,.38f},{.40f,.46f,.10f}),upholstery,
             felt,{4,5},.025f,14,0,1,.95f,2);
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

    void roomTableLegs(const glm::mat4& root, bool foreground) {
        const glm::vec3 wood=foreground?glm::vec3(.115f,.040f,.055f):glm::vec3(.055f,.025f,.040f);
        const glm::vec3 brass=foreground?glm::vec3(.38f,.26f,.12f):glm::vec3(.20f,.14f,.075f);
        for (const float side : {-1.0f,1.0f})
            for (const float front : {-1.0f,1.0f}) {
                const glm::vec3 foot(side*2.88f,-.75f,front*1.47f);
                const glm::vec3 worldFoot=glm::vec3(root*glm::vec4(foot,1));
                roomContactShadow(worldFoot,{.18f,.16f},.23f);
                beam(foot,{side*2.73f,-.23f,front*1.35f},.095f,wood,root);
                draw(cylinder,root*transform(foot,{.13f,.04f,.13f}),brass,0,glm::vec2(1),.18f,40);
                draw(cylinder,root*transform({side*2.76f,-.32f,front*1.38f},{.105f,.045f,.105f}),
                     brass,0,glm::vec2(1),.18f,40);
            }
    }

    void roomEmptyTable(glm::vec3 center, float yaw) {
        const glm::mat4 root=transform(center,glm::vec3(1),yaw);
        roomContactShadow(center,{4.25f,3.05f},.16f,yaw);
        roomTableLegs(root,false);
        // Identical world dimensions to the foreground table; distance alone reduces it.
        draw(cylinder,root*transform({0,-.16f,0},{4.2f,.18f,3.0f}),{.060f,.024f,.037f},
             0,glm::vec2(1),.07f,28);
        draw(cylinder,root*transform({0,-.085f,0},{4.05f,.06f,2.85f}),{.25f,.17f,.085f},
             0,glm::vec2(1),.12f,36);
        draw(cylinder,root*transform({0,-.045f,0},{3.95f,.04f,2.75f}),{.018f,.035f,.027f},
             0,glm::vec2(1),.03f,20);
        draw(cylinder,root*transform({0,-.025f,0},{3.83f,.04f,2.62f}),{.023f,.125f,.073f},
             felt,{14,11},.015f,10,0,1,.9f);
    }

    void drawRoom(float time, bool animate) {
        const glm::vec3 bronze{.32f,.225f,.12f},stone{.095f,.072f,.10f};
        draw(cube,transform({0,-.92f,-10},{50,.25f,64}),{.035f,.025f,.040f});
        draw(cube,transform({0,-.79f,-10},{50,.03f,64}),{.12f,.10f,.12f},
             0,glm::vec2(1),.16f,48,0,1,0,3);

        // Freestanding columns and unevenly gathered wings leave the central aisle open.
        const glm::vec3 columns[]={{-6.6f,0,-7.5f},{6.75f,0,-7.5f},
                                   {-14.1f,0,-23.0f},{15.0f,0,-27.0f}};
        for (const auto& p : columns) {
            draw(cylinder,transform({p.x,.91f,p.z},{.20f,3.36f,.20f}),stone);
            draw(cylinder,transform({p.x,-.68f,p.z},{.31f,.18f,.31f}),bronze*.70f);
            draw(cylinder,transform({p.x,2.55f,p.z},{.29f,.12f,.29f}),bronze*.65f);
        }
        draw(curtain,transform({-7.65f,-.0775f,-7.85f},{.48f,.93f,1},.06f),
             {.22f,.028f,.075f},felt,{6,8},.025f,14,0,1,1,2);
        draw(curtain,transform({7.75f,-.01f,-8.15f},{.58f,1.02f,1},-.08f),
             {.12f,.035f,.17f},felt,{7,8},.025f,14,0,1,1,2);
        draw(cube,transform({-7.65f,1.28f,-7.53f},{.92f,.038f,.035f}),bronze*.65f);
        draw(cube,transform({7.75f,.66f,-7.81f},{1.20f,.038f,.035f}),bronze*.60f);

        const glm::vec3 leftTable{-8.2f,0,-15.5f},rightTable{9.5f,0,-20.0f};
        roomEmptyTable(leftTable,.18f);
        roomEmptyTable(rightTable,-.24f);
        roomChair(leftTable+glm::vec3(-1.75f,0,3.75f),.19f,{.11f,.026f,.054f});
        roomChair(leftTable+glm::vec3(1.45f,0,3.60f),-.28f,{.09f,.032f,.105f});
        roomChair(rightTable+glm::vec3(-1.50f,0,4.40f),.35f,{.10f,.028f,.070f});
        roomLamp({-4.7f,1.25f,-6.4f},roomLampBrightness(time,animate));

        for (const float side : {-1.0f,1.0f}) {
            const float x=side*5.45f;
            draw(cylinder,transform({x,-.64f,-2.6f},{.27f,.14f,.27f}),bronze,0,glm::vec2(1),.26f,48);
            beam({x,-.61f,-2.6f},{x,1.03f,-2.6f},.048f,bronze);
            for (int branch=-1;branch<=1;++branch) {
                const float bx=x+branch*.25f,by=branch==0?1.18f:.96f;
                beam({x,.70f,-2.6f},{bx,by,-2.6f},.030f,bronze);
                draw(cylinder,transform({bx,by,-2.6f},{.11f,.05f,.11f}),bronze);
                draw(cylinder,transform({bx,by+.23f,-2.6f},{.063f,.42f,.063f}),{.67f,.57f,.38f});
                const float drift=animate?std::sin(time*1.3f+bx+branch*.8f)*.006f:0;
                draw(sphere,transform({bx+drift*.3f,by+.51f,-2.6f},{.040f,.10f+drift,.040f}),
                     {1,.44f,.10f},0,glm::vec2(1),0,8,1.35f);
            }
        }
        roomTableLegs(glm::mat4(1),true);
    }
