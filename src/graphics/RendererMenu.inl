    // Exterior geometry is kept separate from the preserved theatre/table scene.
    void drawCasino(const Scene& scene, bool animate) {
        const auto building = palaceTransform();
        const glm::vec3 stone{.43f,.40f,.43f}, ivory{.71f,.64f,.52f};
        const glm::vec3 plum{.105f,.043f,.14f}, crimson{.30f,.025f,.067f};
        const glm::vec3 gold{.69f,.46f,.19f}, warm{1,.63f,.25f};
        const auto block = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c, int material=0, float glow=0.0f) {
            draw(cube,building*transform(p,s),c,0,glm::vec2(1),.24f,64,glow,1,0,material);
        };
        const auto ball = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c) {
            draw(sphere,building*transform(p,s),c,0,glm::vec2(1),.30f,64);
        };
        // All paving, curbs and traffic share the palace basis. The paving material
        // dissolves at distance, rather than revealing the edge of a rectangular mesh.
        block({0,-.14f,0},{180,.20f,180},{.058f,.067f,.075f},16);
        draw(cylinder,building*transform({0,-.009f,4.2f},{8.2f,.06f,5.0f}),
             {.24f,.23f,.25f},0,glm::vec2(1),.30f,76,0,1,0,17);
        constexpr float streetCenter=10.55f, streetWidth=3.25f;
        block({0,-.005f,streetCenter},{100,.035f,streetWidth},{.054f,.060f,.075f},19);
        for(float side:{-1.0f,1.0f})
            block({0,.035f,streetCenter+side*streetWidth*.5f},{100,.07f,.14f},{.28f,.27f,.28f},16);
        for(int i=-28;i<=28;++i)
            block({i*1.65f,.018f,streetCenter},{.83f,.008f,.055f},{.42f,.34f,.20f},16);
        // Red arrival carpet and shallow, broad steps sit on a rounded drop-off court.
        block({0,.037f,5.40f},{1.65f,.025f,6.8f},crimson*.62f);
        for(float side:{-1.0f,1.0f})
            block({side*.79f,.054f,5.40f},{.027f,.008f,6.8f},gold);
        for(int step=0;step<3;++step)
            block({0,.08f+step*.08f,2.70f-step*.28f},{3.45f-step*.23f,.16f,.78f},ivory*.72f);

        // A coherent two-storey stone elevation: solid wings, repeating pilasters,
        // deep cornices and a raised central entrance pavilion.
        for(float side:{-1.0f,1.0f}) {
            block({side*2.67f,2.48f,-.08f},{3.48f,4.66f,3.85f},stone,11);
            block({side*2.67f,.40f,1.94f},{3.53f,.48f,.29f},plum);
            block({side*2.67f,2.48f,1.96f},{3.53f,.13f,.30f},ivory*.8f);
            block({side*2.67f,4.66f,-.05f},{3.66f,.24f,4.09f},ivory);
            block({side*2.67f,4.82f,-.05f},{3.78f,.08f,4.20f},gold);
            block({side*2.67f,4.94f,-.05f},{3.57f,.17f,3.96f},plum);
        }
        for(float x:{-4.34f,-2.60f,2.60f,4.34f}) {
            block({x,2.44f,1.98f},{.17f,4.14f,.28f},ivory*.8f);
            block({x,.43f,2.04f},{.31f,.22f,.38f},ivory);
            block({x,4.43f,2.04f},{.32f,.16f,.40f},ivory);
            for(float y:{.82f,1.24f,1.66f,2.09f})
                block({x,y,2.13f},{.18f,.025f,.03f},stone*.60f);
        }
        // Continuous solid masonry above the door removes the former .16-unit hole.
        block({0,3.91f,-.05f},{2.10f,2.60f,3.90f},stone,11);
        block({0,5.16f,-.05f},{2.45f,.20f,4.15f},ivory);
        block({0,5.28f,-.05f},{2.55f,.055f,4.22f},gold);
        // An unlit, enclosed throat. Its surfaces extinguish with depth and cannot
        // inherit the exterior's ambient/fog, even during the camera push.
        block({0,.24f,-2.1f},{1.86f,.14f,8.30f},{.025f,.012f,.018f},18);
        block({0,1.47f,-6.20f},{1.87f,2.60f,.20f},{0,0,0},18);
        block({0,2.65f,-2.10f},{1.87f,.16f,8.30f},{.016f,.010f,.018f},18);
        for(float side:{-1.0f,1.0f}) {
            block({side*.965f,1.47f,-2.1f},{.15f,2.55f,8.3f},{.021f,.012f,.022f},18);
            block({side*1.05f,1.56f,2.03f},{.25f,2.77f,.40f},ivory);
            block({side*1.05f,.43f,2.10f},{.39f,.27f,.52f},ivory);
            block({side*1.05f,2.85f,2.10f},{.39f,.14f,.52f},gold);
        }
        // A dark solid fanlight provides a high arched portal without exposing sky.
        draw(arch,building*transform({0,2.85f,2.15f},{1.20f,.74f,.62f}),ivory,0,glm::vec2(1),.28f,64);
        draw(arch,building*transform({0,2.85f,2.19f},{1.08f,.64f,.46f}),gold,0,glm::vec2(1),.42f,76);
        block({0,2.72f,2.04f},{1.86f,.20f,.27f},plum);
        for(int ray=0;ray<=8;++ray) {
            const float a=ray*pi/8;
            beam({0,2.74f,2.20f},{std::cos(a)*.88f,2.74f+std::sin(a)*.57f,2.20f},.013f,gold,building);
        }
        // Exterior is +Z: both free edges swing towards the forecourt, away from
        // the dark hall. EntranceTransition remains the sole animation time source.
        for(float side:{-1.0f,1.0f}) {
            auto leaf=building*glm::translate(glm::mat4(1),{side*.87f,1.47f,1.96f});
            leaf=glm::rotate(leaf,side*std::clamp(scene.doorOpen,0.0f,1.0f)*1.48f,glm::vec3(0,1,0));
            draw(cube,leaf*transform({-side*.43f,0,0},{.86f,2.30f,.12f}),crimson,0,glm::vec2(1),.26f,72);
            // Both faces are finished; the reverse becomes visible during opening.
            for(float face:{-1.0f,1.0f}) {
                draw(cube,leaf*transform({-side*.43f,0,face*.075f},{.72f,2.12f,.025f}),gold);
                draw(cube,leaf*transform({-side*.43f,0,face*.094f},{.65f,2.04f,.018f}),plum);
                for(float y:{-.62f,0.0f,.62f})
                    diamond({-side*.43f,y,face*.111f},.235f,gold,leaf);
                draw(sphere,leaf*transform({-side*.73f,-.05f,face*.15f},{.035f,.09f,.035f}),gold);
            }
        }

        // Recessed arched windows, opaque mullions and deep sills. Light source
        // coordinates match the menu-light shader in palace-local space.
        for(float x:{-3.45f,-1.80f,1.80f,3.45f}) for(float y:{1.44f,3.52f}) {
            block({x,y,1.88f},{.90f,1.40f,.16f},plum);
            block({x,y-.04f,1.995f},{.62f,1.13f,.035f},warm,20,1);
            draw(sphere,building*transform({x,y+.49f,1.995f},{.31f,.27f,.019f}),warm,
                 0,glm::vec2(1),.10f,32,1,1,0,20);
            draw(arch,building*transform({x,y+.49f,2.04f},{.42f,.36f,.35f}),ivory,
                 0,glm::vec2(1),.28f,64);
            for(float side:{-1.0f,1.0f})
                block({x+side*.39f,y-.06f,2.07f},{.085f,1.14f,.18f},ivory);
            block({x,y+.03f,2.10f},{.038f,1.25f,.08f},plum);
            block({x,y-.15f,2.10f},{.64f,.045f,.08f},plum);
            block({x,y-.66f,2.10f},{1.0f,.12f,.43f},ivory);
            block({x,y-.78f,2.0f},{.74f,.08f,.23f},stone*.65f);
        }
        // Slate mansards replace the literal jester hats. Ribs and dormer crowns
        // connect the roof silhouette to the ordered facade below it.
        for(float side:{-1.0f,1.0f}) {
            draw(palaceRoof,building*transform({side*2.67f,5.0f,-.05f},{1.78f,.83f,1.97f}),{.16f,.13f,.20f},
                 0,glm::vec2(1),.27f,72,0,1,0,21);
            block({side*2.67f,5.84f,-.05f},{2.50f,.055f,2.77f},gold);
            for(int rib=-2;rib<=2;++rib) {
                const float x=side*2.67f+rib*.58f;
                beam({x,5.0f,1.94f},{side*2.67f+rib*.40f,5.82f,1.33f},.012f,gold*.7f,building);
            }
        }
        ball({0,5.30f,-.05f},{1.18f,.79f,1.38f},plum);
        for(int rib=0;rib<12;++rib) {
            const float a=rib*pi/6;
            beam({std::cos(a)*1.12f,5.35f,-.05f+std::sin(a)*1.31f},
                 {std::cos(a)*.45f,6.0f,-.05f+std::sin(a)*.53f},.016f,gold,building);
        }
        ball({0,6.10f,-.05f},{.10f,.15f,.10f},gold);
        diamond({0,6.36f,-.05f},.19f,gold,building);
        // Porcelain house emblem belongs to the central pavilion, with a modest
        // three-bell crown and a slim gold diamond frieze rather than roof hats.
        auto seal=building*transform({0,4.17f,2.11f},{1.90f,1.90f,.85f});
        draw(leadMask,seal,{.92f,.85f,.70f},0,glm::vec2(1),.32f,84,.05f,1,0,8);
        draw(leadMaskTrim,seal,gold,0,glm::vec2(1),.62f,84);
        for(float x:{-.47f,0.0f,.47f}) {
            const float y=4.65f+(x==0?.14f:0);
            beam({x*.67f,4.48f,2.13f},{x,y,2.13f},.018f,gold,building);
            ball({x,y,2.13f},{.058f,.072f,.058f},gold);
        }
        for(float side:{-1.0f,1.0f}) {
            for(int i=0;i<6;++i) diamond({side*(1.25f+i*.56f),4.61f,2.09f},.080f,gold,building);
            draw(cube,building*transform({side*2.60f,3.46f,2.21f},{.42f,1.06f,.03f}),
                 crimson,felt,{2,3},.05f,24,0,1,.40f,4);
            diamond({side*2.60f,3.47f,2.24f},.16f,gold,building);
        }

        // Four freestanding lanterns: visible luminous glass, dark corner bars,
        // brass shoulders and a down-facing cap match the shader's point lights.
        for(float side:{-1.0f,1.0f}) for(float z:{4.3f,7.65f}) {
            const float x=side*3.1f;
            draw(cylinder,building*transform({x,.10f,z},{.18f,.18f,.18f}),plum);
            beam({x,.16f,z},{x,2.03f,z},.047f,gold*.6f,building);
            block({x,2.22f,z},{.30f,.43f,.30f},warm,20,1);
            for(float dx:{-.155f,.155f}) for(float dz:{-.155f,.155f})
                beam({x+dx,1.98f,z+dz},{x+dx,2.45f,z+dz},.018f,plum,building);
            block({x,1.98f,z},{.37f,.07f,.37f},gold);
            ball({x,2.48f,z},{.25f,.12f,.25f},plum);
            ball({x,2.63f,z},{.045f,.075f,.045f},gold);
        }
        // Trimmed planting islands and layered trees frame an actual hotel drive.
        // Their low silhouettes fade separately from the brightly lit palace.
        for(float side:{-1.0f,1.0f}) {
            for(float z:{1.0f,4.5f,7.5f}) {
                draw(cylinder,building*transform({side*6.5f,.08f,z},{1.22f,.18f,.85f}),stone*.7f,
                     0,glm::vec2(1),.12f,32,0,1,0,16);
                for(int bush=0;bush<4;++bush)
                    draw(sphere,building*transform({side*6.5f+(bush-1.5f)*.45f,.39f,z},
                         {.40f,.38f,.61f}),{.055f,.13f,.105f},0,glm::vec2(1),.03f,16,0,1,0,16);
            }
            for(int tree=0;tree<7;++tree) {
                const float x=side*(7.9f+tree*1.42f), z=1.5f-tree*1.8f;
                const float h=2.5f+.30f*std::sin(tree*2.1f);
                beam({x,.02f,z},{x,h,z},.07f,{.08f,.065f,.073f},building);
                for(int crown=0;crown<5;++crown) {
                    const float a=crown*2*pi/5;
                    draw(treeCrown,building*transform({x+std::cos(a)*.42f,h+std::sin(a*2)*.25f,z+std::sin(a)*.35f},
                         {.78f,.84f,.73f}),{.075f+.003f*crown,.14f+.004f*crown,.12f},
                         0,glm::vec2(1),.025f,16,0,1,0,22);
                }
            }
        }
        // Original sedan, wheels and headlight pools restored, on the outer lane.
        drawMenuSedan(building*glm::translate(glm::mat4(1),{0,0,5.0f}),scene.time,animate);
    }
