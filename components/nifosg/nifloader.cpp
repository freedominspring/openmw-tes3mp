#include "nifloader.hpp"

#include <osg/Matrixf>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Array>

#include <osg/io_utils>

// resource
#include <components/misc/stringops.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/texturemanager.hpp>

// skel
#include <osgAnimation/Skeleton>
#include <osgAnimation/Bone>
#include <osgAnimation/RigGeometry>
#include <osgAnimation/MorphGeometry>
#include <osgAnimation/BoneMapVisitor>

// particle
#include <osgParticle/ParticleSystem>
#include <osgParticle/ParticleSystemUpdater>
#include <osgParticle/ConstantRateCounter>
#include <osgParticle/ModularEmitter>
#include <osgParticle/Shooter>
#include <osgParticle/BoxPlacer>
#include <osgParticle/ModularProgram>

#include <osg/BlendFunc>
#include <osg/AlphaFunc>
#include <osg/Depth>
#include <osg/PolygonMode>
#include <osg/FrontFace>
#include <osg/Stencil>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/TexEnvCombine>

#include <components/nif/node.hpp>

#include "particle.hpp"
#include "userdata.hpp"

namespace
{
    osg::Matrixf toMatrix(const Nif::Transformation& nifTrafo)
    {
        osg::Matrixf transform;
        transform.setTrans(nifTrafo.pos);

        for (int i=0;i<3;++i)
            for (int j=0;j<3;++j)
                transform(j,i) = nifTrafo.rotation.mValues[i][j] * nifTrafo.scale; // NB column/row major difference

        return transform;
    }

    osg::Matrixf getWorldTransform(const Nif::Node* node)
    {
        if(node->parent != NULL)
            return toMatrix(node->trafo) * getWorldTransform(node->parent);
        return toMatrix(node->trafo);
    }

    void getAllNiNodes(const Nif::Node* node, std::vector<int>& outIndices)
    {
        const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(node);
        if (ninode)
        {
            outIndices.push_back(ninode->recIndex);
            for (unsigned int i=0; i<ninode->children.length(); ++i)
                if (!ninode->children[i].empty())
                    getAllNiNodes(ninode->children[i].getPtr(), outIndices);
        }
    }

    osg::BlendFunc::BlendFuncMode getBlendMode(int mode)
    {
        switch(mode)
        {
        case 0: return osg::BlendFunc::ONE;
        case 1: return osg::BlendFunc::ZERO;
        case 2: return osg::BlendFunc::SRC_COLOR;
        case 3: return osg::BlendFunc::ONE_MINUS_SRC_COLOR;
        case 4: return osg::BlendFunc::DST_COLOR;
        case 5: return osg::BlendFunc::ONE_MINUS_DST_COLOR;
        case 6: return osg::BlendFunc::SRC_ALPHA;
        case 7: return osg::BlendFunc::ONE_MINUS_SRC_ALPHA;
        case 8: return osg::BlendFunc::DST_ALPHA;
        case 9: return osg::BlendFunc::ONE_MINUS_DST_ALPHA;
        case 10: return osg::BlendFunc::SRC_ALPHA_SATURATE;
        default:
            std::cerr<< "Unexpected blend mode: "<< mode << std::endl;
            return osg::BlendFunc::SRC_ALPHA;
        }
    }

    osg::AlphaFunc::ComparisonFunction getTestMode(int mode)
    {
        switch (mode)
        {
        case 0: return osg::AlphaFunc::ALWAYS;
        case 1: return osg::AlphaFunc::LESS;
        case 2: return osg::AlphaFunc::EQUAL;
        case 3: return osg::AlphaFunc::LEQUAL;
        case 4: return osg::AlphaFunc::GREATER;
        case 5: return osg::AlphaFunc::NOTEQUAL;
        case 6: return osg::AlphaFunc::GEQUAL;
        case 7: return osg::AlphaFunc::NEVER;
        default:
            std::cerr << "Unexpected blend mode: " << mode << std::endl;
            return osg::AlphaFunc::LEQUAL;
        }
    }

    osg::Stencil::Function getStencilFunction(int func)
    {
        switch (func)
        {
        case 0: return osg::Stencil::NEVER;
        case 1: return osg::Stencil::LESS;
        case 2: return osg::Stencil::EQUAL;
        case 3: return osg::Stencil::LEQUAL;
        case 4: return osg::Stencil::GREATER;
        case 5: return osg::Stencil::NOTEQUAL;
        case 6: return osg::Stencil::GEQUAL;
        case 7: return osg::Stencil::NEVER; // NifSkope says this is GL_ALWAYS, but in MW it's GL_NEVER
        default:
            std::cerr << "Unexpected stencil function: " << func << std::endl;
            return osg::Stencil::NEVER;
        }
    }

    osg::Stencil::Operation getStencilOperation(int op)
    {
        switch (op)
        {
        case 0: return osg::Stencil::KEEP;
        case 1: return osg::Stencil::ZERO;
        case 2: return osg::Stencil::REPLACE;
        case 3: return osg::Stencil::INCR;
        case 4: return osg::Stencil::DECR;
        case 5: return osg::Stencil::INVERT;
        default:
            std::cerr << "Unexpected stencil operation: " << op << std::endl;
            return osg::Stencil::KEEP;
        }
    }

    // Collect all properties affecting the given node that should be applied to an osg::Material.
    void collectMaterialProperties(const Nif::Node* nifNode, std::vector<const Nif::Property*>& out)
    {
        const Nif::PropertyList& props = nifNode->props;
        for (size_t i = 0; i <props.length();++i)
        {
            if (!props[i].empty())
            {
                switch (props[i]->recType)
                {
                case Nif::RC_NiMaterialProperty:
                case Nif::RC_NiVertexColorProperty:
                case Nif::RC_NiSpecularProperty:
                    out.push_back(props[i].getPtr());
                    break;
                default:
                    break;
                }
            }
        }
        if (nifNode->parent)
            collectMaterialProperties(nifNode->parent, out);
    }

    // NodeCallback used to update the bone matrices in skeleton space as needed for skinning.
    // Must be set on a Bone.
    class UpdateBone : public osg::NodeCallback
    {
    public:
        UpdateBone() {}
        UpdateBone(const UpdateBone& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
            : osg::Object(copy, copyop), osg::NodeCallback(copy, copyop)
        {
        }

        META_Object(NifOsg, UpdateBone)

        // Callback method called by the NodeVisitor when visiting a node.
        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            if (nv && nv->getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR)
            {
                osgAnimation::Bone* b = dynamic_cast<osgAnimation::Bone*>(node);
                if (!b)
                {
                    OSG_WARN << "Warning: UpdateBone set on non-Bone object." << std::endl;
                    return;
                }

                osgAnimation::Bone* parent = b->getBoneParent();
                if (parent)
                    b->setMatrixInSkeletonSpace(b->getMatrixInBoneSpace() * parent->getMatrixInSkeletonSpace());
                else
                    b->setMatrixInSkeletonSpace(b->getMatrixInBoneSpace());
            }
            traverse(node,nv);
        }
    };

    // NodeCallback used to have a transform always oriented towards the camera. Can have translation and scale
    // set just like a regular MatrixTransform, but the rotation set will be overridden in order to face the camera.
    class BillboardCallback : public osg::NodeCallback
    {
    public:
        BillboardCallback()
        {
        }
        BillboardCallback(const BillboardCallback& copy, const osg::CopyOp& copyop)
            : osg::NodeCallback(copy, copyop)
        {
        }

        META_Object(NifOsg, BillboardCallback)

        virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(nv);
            osg::MatrixTransform* billboardNode = dynamic_cast<osg::MatrixTransform*>(node);
            if (billboardNode && cv)
            {
                osg::Matrix modelView = *cv->getModelViewMatrix();

                // attempt to preserve scale
                float mag[3];
                for (int i=0;i<3;++i)
                {
                    mag[i] = std::sqrt(modelView(0,i) * modelView(0,i) + modelView(1,i) * modelView(1,i) + modelView(2,i) * modelView(2,i));
                }

                modelView.setRotate(osg::Quat());
                modelView(0,0) = mag[0];
                modelView(1,1) = mag[1];
                modelView(2,2) = mag[2];

                cv->pushModelViewMatrix(new osg::RefMatrix(modelView), osg::Transform::RELATIVE_RF);

                traverse(node, nv);

                cv->popModelViewMatrix();
            }
            else
                traverse(node, nv);
        }
    };

    // NodeCallback used to set the inverse of the parent bone's matrix in skeleton space
    // on the MatrixTransform that the NodeCallback is attached to. This is used so we can
    // attach skinned meshes to their actual parent node, while still having the skinning
    // work in skeleton space as expected.
    // Must be set on a MatrixTransform.
    class InvertBoneMatrix : public osg::NodeCallback
    {
    public:
        InvertBoneMatrix() {}

        InvertBoneMatrix(const InvertBoneMatrix& copy, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
            : osg::Object(copy, copyop), osg::NodeCallback(copy, copyop) {}

        META_Object(NifOsg, InvertBoneMatrix)

        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            if (nv && nv->getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR)
            {
                osg::NodePath path = nv->getNodePath();
                path.pop_back();

                osg::MatrixTransform* trans = dynamic_cast<osg::MatrixTransform*>(node);

                for (osg::NodePath::iterator it = path.begin(); it != path.end(); ++it)
                {
                    if (dynamic_cast<osgAnimation::Skeleton*>(*it))
                    {
                        path.erase(path.begin(), it+1);
                        // the bone's transform in skeleton space
                        osg::Matrix boneMat = osg::computeLocalToWorld( path );
                        trans->setMatrix(osg::Matrix::inverse(boneMat));
                        break;
                    }
                }
            }
            traverse(node,nv);
        }
    };


    // NodeVisitor that adds keyframe controllers to an existing scene graph, used when loading .kf files
    /*
    class LoadKfVisitor : public osg::NodeVisitor
    {
    public:
        LoadKfVisitor(std::map<std::string, const Nif::NiKeyframeController*> map, int sourceIndex)
            : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            , mMap(map)
            , mSourceIndex(sourceIndex)
        {
        }

        void apply(osg::Node &node)
        {
            std::map<std::string, const Nif::NiKeyframeController*>::const_iterator found = mMap.find(node.getName());
            if (node.asTransform() && found != mMap.end())
            {
                const Nif::NiKeyframeController* keyframectrl = found->second;

                osg::ref_ptr<NifOsg::SourcedKeyframeController> callback(new NifOsg::SourcedKeyframeController(keyframectrl->data.getPtr(), mSourceIndex));
                callback->mFunction = boost::shared_ptr<NifOsg::ControllerFunction>(new NifOsg::ControllerFunction(keyframectrl));

                // Insert in front of the callback list, to make sure UpdateBone is last.
                // The order of SourcedKeyframeControllers doesn't matter since only one of them should be enabled at a time.
                osg::ref_ptr<osg::NodeCallback> old = node.getUpdateCallback();
                node.setUpdateCallback(callback);
                callback->setNestedCallback(old);
            }

            traverse(node);
        }

    private:
        std::map<std::string, const Nif::NiKeyframeController*> mMap;
        int mSourceIndex;
    };
    */

    // Node callback used to dirty a RigGeometry's bounding box every frame, so that RigBoundingBoxCallback updates.
    // This has to be attached to the geode, because the RigGeometry's Drawable::UpdateCallback is already used internally and not extensible.
    // Kind of awful, not sure of a better way to do this.
    class DirtyBoundCallback : public osg::NodeCallback
    {
    public:
        DirtyBoundCallback()
        {
        }
        DirtyBoundCallback(const DirtyBoundCallback& copy, const osg::CopyOp& copyop)
            : osg::NodeCallback(copy, copyop)
        {
        }

        META_Object(NifOsg, DirtyBoundCallback)

        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            osg::Geode* geode = node->asGeode();
            if (geode && geode->getNumDrawables())
            {
                geode->getDrawable(0)->dirtyBound();
            }
            traverse(node, nv);
        }
    };

    struct FindNearestParentSkeleton : public osg::NodeVisitor
    {
        osg::ref_ptr<osgAnimation::Skeleton> _root;
        FindNearestParentSkeleton() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_PARENTS) {}
        void apply(osg::Transform& node)
        {
            if (_root.valid())
                return;
            _root = dynamic_cast<osgAnimation::Skeleton*>(&node);
            traverse(node);
        }
    };

    // RigGeometry is skinned from a CullCallback to prevent unnecessary updates of culled rig geometries.
    // Note: this assumes only one cull thread is using the given RigGeometry, there would be a race condition otherwise.
    struct UpdateRigGeometry : public osg::Drawable::CullCallback
    {
        virtual bool cull(osg::NodeVisitor *, osg::Drawable * drw, osg::State *) const
        {
            osgAnimation::RigGeometry* geom = static_cast<osgAnimation::RigGeometry*>(drw);
            if (!geom)
                return false;
            if (!geom->getSkeleton() && !geom->getParents().empty())
            {
                FindNearestParentSkeleton finder;
                if (geom->getParents().size() > 1)
                    osg::notify(osg::WARN) << "A RigGeometry should not have multi parent ( " << geom->getName() << " )" << std::endl;
                geom->getParents()[0]->accept(finder);

                if (!finder._root.valid())
                {
                    osg::notify(osg::WARN) << "A RigGeometry did not find a parent skeleton for RigGeometry ( " << geom->getName() << " )" << std::endl;
                    return false;
                }
                geom->buildVertexInfluenceSet();
                geom->setSkeleton(finder._root.get());
            }

            if (!geom->getSkeleton())
                return false;

            if (geom->getNeedToComputeMatrix())
                geom->computeMatrixFromRootSkeleton();

            geom->update();

            return false;
        }
    };

    class RigBoundingBoxCallback : public osg::Drawable::ComputeBoundingBoxCallback
    {
    public:
        RigBoundingBoxCallback()
            : mBoneMapInit(false)
        {
        }
        RigBoundingBoxCallback(const RigBoundingBoxCallback& copy, const osg::CopyOp& copyop)
            : osg::Drawable::ComputeBoundingBoxCallback(copy, copyop)
            , mBoneMapInit(false)
            , mBoundSpheres(copy.mBoundSpheres)
        {
        }

        META_Object(NifOsg, RigBoundingBoxCallback)

        void addBoundSphere(const std::string& bonename, const osg::BoundingSphere& sphere)
        {
            mBoundSpheres[bonename] = sphere;
        }

        // based off code in osg::Transform
        void transformBoundingSphere (const osg::Matrix& matrix, osg::BoundingSphere& bsphere) const
        {
            osg::BoundingSphere::vec_type xdash = bsphere._center;
            xdash.x() += bsphere._radius;
            xdash = xdash*matrix;

            osg::BoundingSphere::vec_type ydash = bsphere._center;
            ydash.y() += bsphere._radius;
            ydash = ydash*matrix;

            osg::BoundingSphere::vec_type zdash = bsphere._center;
            zdash.z() += bsphere._radius;
            zdash = zdash*matrix;

            bsphere._center = bsphere._center*matrix;

            xdash -= bsphere._center;
            osg::BoundingSphere::value_type len_xdash = xdash.length();

            ydash -= bsphere._center;
            osg::BoundingSphere::value_type len_ydash = ydash.length();

            zdash -= bsphere._center;
            osg::BoundingSphere::value_type len_zdash = zdash.length();

            bsphere._radius = len_xdash;
            if (bsphere._radius<len_ydash) bsphere._radius = len_ydash;
            if (bsphere._radius<len_zdash) bsphere._radius = len_zdash;
        }

        virtual osg::BoundingBox computeBound(const osg::Drawable& drawable) const
        {
            osg::BoundingBox box;

            const osgAnimation::RigGeometry* rig = dynamic_cast<const osgAnimation::RigGeometry*>(&drawable);
            if (!rig)
            {
                std::cerr << "Warning: RigBoundingBoxCallback set on non-rig" << std::endl;
                return box;
            }

            if (!mBoneMapInit)
            {
                initBoneMap(rig);
            }

            for (std::map<osgAnimation::Bone*, osg::BoundingSphere>::const_iterator it = mBoneMap.begin();
                 it != mBoneMap.end(); ++it)
            {
                osgAnimation::Bone* bone = it->first;
                osg::BoundingSphere bs = it->second;
                transformBoundingSphere(bone->getMatrixInSkeletonSpace(), bs);
                box.expandBy(bs);
            }

            return box;
        }

        void initBoneMap(const osgAnimation::RigGeometry* rig) const
        {
            if (!rig->getSkeleton())
            {
                // may happen before the first frame update, but we're not animating yet, so no need for a bounding box
                return;
            }

            osgAnimation::BoneMapVisitor mapVisitor;
            {
                // const_cast necessary because there does not seem to be a const variant of NodeVisitor.
                // Don't worry, we're not actually changing the skeleton.
                osgAnimation::Skeleton* skel = const_cast<osgAnimation::Skeleton*>(rig->getSkeleton());
                skel->accept(mapVisitor);
            }

            for (osgAnimation::BoneMap::const_iterator it = mapVisitor.getBoneMap().begin(); it != mapVisitor.getBoneMap().end(); ++it)
            {
                std::map<std::string, osg::BoundingSphere>::const_iterator found = mBoundSpheres.find(it->first);
                if (found != mBoundSpheres.end()) // not all bones have to be used for skinning
                    mBoneMap[it->second.get()] = found->second;
            }

            mBoneMapInit = true;
        }

    private:
        mutable bool mBoneMapInit;
        mutable std::map<osgAnimation::Bone*, osg::BoundingSphere> mBoneMap;

        std::map<std::string, osg::BoundingSphere> mBoundSpheres;
    };

    void extractTextKeys(const Nif::NiTextKeyExtraData *tk, NifOsg::TextKeyMap &textkeys)
    {
        for(size_t i = 0;i < tk->list.size();i++)
        {
            const std::string &str = tk->list[i].text;
            std::string::size_type pos = 0;
            while(pos < str.length())
            {
                if(::isspace(str[pos]))
                {
                    pos++;
                    continue;
                }

                std::string::size_type nextpos = std::min(str.find('\r', pos), str.find('\n', pos));
                if(nextpos != std::string::npos)
                {
                    do {
                        nextpos--;
                    } while(nextpos > pos && ::isspace(str[nextpos]));
                    nextpos++;
                }
                else if(::isspace(*str.rbegin()))
                {
                    std::string::const_iterator last = str.end();
                    do {
                        --last;
                    } while(last != str.begin() && ::isspace(*last));
                    nextpos = std::distance(str.begin(), ++last);
                }
                std::string result = str.substr(pos, nextpos-pos);
                textkeys.insert(std::make_pair(tk->list[i].time, Misc::StringUtils::toLower(result)));

                pos = nextpos;
            }
        }
    }
}

namespace NifOsg
{

    bool Loader::sShowMarkers = false;

    void Loader::setShowMarkers(bool show)
    {
        sShowMarkers = show;
    }

    bool Loader::getShowMarkers()
    {
        return sShowMarkers;
    }

    class LoaderImpl
    {
    public:
        static void loadKf(Nif::NIFFilePtr nif, KeyframeHolder& target)
        {
            if(nif->numRoots() < 1)
            {
                nif->warn("Found no root nodes");
                return;
            }

            const Nif::Record *r = nif->getRoot(0);
            assert(r != NULL);

            if(r->recType != Nif::RC_NiSequenceStreamHelper)
            {
                nif->warn("First root was not a NiSequenceStreamHelper, but a "+
                          r->recName+".");
                return;
            }
            const Nif::NiSequenceStreamHelper *seq = static_cast<const Nif::NiSequenceStreamHelper*>(r);

            Nif::ExtraPtr extra = seq->extra;
            if(extra.empty() || extra->recType != Nif::RC_NiTextKeyExtraData)
            {
                nif->warn("First extra data was not a NiTextKeyExtraData, but a "+
                          (extra.empty() ? std::string("nil") : extra->recName)+".");
                return;
            }

            extractTextKeys(static_cast<const Nif::NiTextKeyExtraData*>(extra.getPtr()), target.mTextKeys);

            extra = extra->extra;
            Nif::ControllerPtr ctrl = seq->controller;
            for(;!extra.empty() && !ctrl.empty();(extra=extra->extra),(ctrl=ctrl->next))
            {
                if(extra->recType != Nif::RC_NiStringExtraData || ctrl->recType != Nif::RC_NiKeyframeController)
                {
                    nif->warn("Unexpected extra data "+extra->recName+" with controller "+ctrl->recName);
                    continue;
                }

                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;

                const Nif::NiStringExtraData *strdata = static_cast<const Nif::NiStringExtraData*>(extra.getPtr());
                const Nif::NiKeyframeController *key = static_cast<const Nif::NiKeyframeController*>(ctrl.getPtr());

                if(key->data.empty())
                    continue;

                osg::ref_ptr<NifOsg::SourcedKeyframeController> callback(new NifOsg::SourcedKeyframeController(key->data.getPtr()));
                callback->mFunction = boost::shared_ptr<NifOsg::ControllerFunction>(new NifOsg::ControllerFunction(key));

                target.mKeyframeControllers[strdata->string] = callback;
            }
        }

        static osg::ref_ptr<osg::Node> load(Nif::NIFFilePtr nif, Resource::TextureManager* textureManager)
        {
            if (nif->getUseSkinning())
                return loadAsSkeleton(nif, textureManager);

            if (nif->numRoots() < 1)
                nif->fail("Found no root nodes");

            const Nif::Record* r = nif->getRoot(0);

            const Nif::Node* nifNode = dynamic_cast<const Nif::Node*>(r);
            if (nifNode == NULL)
                nif->fail("First root was not a node, but a " + r->recName);

            osg::ref_ptr<TextKeyMapHolder> textkeys (new TextKeyMapHolder);

            osg::ref_ptr<osg::Node> created = handleNode(nifNode, NULL, textureManager, false, std::map<int, int>(), 0, 0, false, &textkeys->mTextKeys);

            created->getOrCreateUserDataContainer()->addUserObject(textkeys);

            return created;
        }

        static osg::ref_ptr<osg::Node> loadAsSkeleton(Nif::NIFFilePtr nif, Resource::TextureManager* textureManager)
        {
            if (nif->numRoots() < 1)
                nif->fail("Found no root nodes");

            const Nif::Record* r = nif->getRoot(0);
            assert(r != NULL);

            const Nif::Node* nifNode = dynamic_cast<const Nif::Node*>(r);
            if (nifNode == NULL)
                nif->fail("First root was not a node, but a " + r->recName);

            osg::ref_ptr<TextKeyMapHolder> textkeys (new TextKeyMapHolder);

            osg::ref_ptr<osgAnimation::Skeleton> skel = new osgAnimation::Skeleton;
            skel->setDefaultUpdateCallback(); // validates the skeleton hierarchy
            handleNode(nifNode, skel, textureManager, true, std::map<int, int>(), 0, 0, false, &textkeys->mTextKeys);

            skel->getOrCreateUserDataContainer()->addUserObject(textkeys);

            return skel;
        }

        static void applyNodeProperties(const Nif::Node *nifNode, osg::Node *applyTo, Resource::TextureManager* textureManager, std::map<int, int>& boundTextures, int animflags)
        {
            const Nif::PropertyList& props = nifNode->props;
            for (size_t i = 0; i <props.length();++i)
            {
                if (!props[i].empty())
                    handleProperty(props[i].getPtr(), applyTo, textureManager, boundTextures, animflags);
            }
        }

        static void setupController(const Nif::Controller* ctrl, Controller* toSetup, int animflags)
        {
            // TODO: uncomment this, currently commented for easier testing
            //bool autoPlay = animflags & Nif::NiNode::AnimFlag_AutoPlay;
            //if (autoPlay)
                toSetup->mSource = boost::shared_ptr<ControllerSource>(new FrameTimeSource);

            toSetup->mFunction = boost::shared_ptr<ControllerFunction>(new ControllerFunction(ctrl));
        }

        static osg::ref_ptr<osg::Node> handleNode(const Nif::Node* nifNode, osg::Group* parentNode, Resource::TextureManager* textureManager, bool createSkeleton,
                                std::map<int, int> boundTextures, int animflags, int particleflags, bool skipMeshes, TextKeyMap* textKeys, osg::Node* rootNode=NULL)
        {
            osg::ref_ptr<osg::MatrixTransform> transformNode;
            if (createSkeleton)
            {
                osgAnimation::Bone* bone = new osgAnimation::Bone;
                transformNode = bone;
                bone->setMatrix(toMatrix(nifNode->trafo));
                bone->setName(nifNode->name);
                bone->setInvBindMatrixInSkeletonSpace(osg::Matrixf::inverse(getWorldTransform(nifNode)));
            }
            else
            {
                transformNode = new osg::MatrixTransform(toMatrix(nifNode->trafo));
            }
            if (nifNode->recType == Nif::RC_NiBillboardNode)
            {
                transformNode->addCullCallback(new BillboardCallback);
            }

            if (parentNode)
                parentNode->addChild(transformNode);

            if (!rootNode)
                rootNode = transformNode;

            // Ignoring name for non-bone nodes for now. We might need it later in isolated cases, e.g. AttachLight.

            // UserData used for a variety of features:
            // - finding the correct emitter node for a particle system
            // - establishing connections to the animated collision shapes, which are handled in a separate loader
            // - finding a random child NiNode in NiBspArrayController
            // - storing the previous 3x3 rotation and scale values for when a KeyframeController wants to
            //   change only certain elements of the 4x4 transform
            transformNode->getOrCreateUserDataContainer()->addUserObject(
                new NodeUserData(nifNode->recIndex, nifNode->trafo.scale, nifNode->trafo.rotation));

            for (Nif::ExtraPtr e = nifNode->extra; !e.empty(); e = e->extra)
            {
                if(e->recType == Nif::RC_NiTextKeyExtraData && textKeys)
                {
                    const Nif::NiTextKeyExtraData *tk = static_cast<const Nif::NiTextKeyExtraData*>(e.getPtr());
                    extractTextKeys(tk, *textKeys);
                }
                else if(e->recType == Nif::RC_NiStringExtraData)
                {
                    const Nif::NiStringExtraData *sd = static_cast<const Nif::NiStringExtraData*>(e.getPtr());
                    // String markers may contain important information
                    // affecting the entire subtree of this obj
                    if(sd->string == "MRK" && !Loader::getShowMarkers())
                    {
                        // Marker objects. These meshes are only visible in the editor.
                        skipMeshes = true;
                    }
                }
            }

            if (nifNode->recType == Nif::RC_NiBSAnimationNode)
                animflags |= nifNode->flags;
            if (nifNode->recType == Nif::RC_NiBSParticleNode)
                particleflags |= nifNode->flags;

            // Hide collision shapes, but don't skip the subgraph
            // We still need to animate the hidden bones so the physics system can access them
            if (nifNode->recType == Nif::RC_RootCollisionNode)
            {
                skipMeshes = true;
                // Leave mask for UpdateVisitor enabled
                transformNode->setNodeMask(0x1);
            }

            // We can skip creating meshes for hidden nodes if they don't have a VisController that
            // might make them visible later
            if (nifNode->flags & Nif::NiNode::Flag_Hidden)
            {
                bool hasVisController = false;
                for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
                    hasVisController = (ctrl->recType == Nif::RC_NiVisController);

                if (!hasVisController)
                    skipMeshes = true; // skip child meshes, but still create the child node hierarchy for animating collision shapes

                // now hide this node, but leave the mask for UpdateVisitor enabled so that KeyframeController works
                transformNode->setNodeMask(0x1);
            }

            applyNodeProperties(nifNode, transformNode, textureManager, boundTextures, animflags);

            if (nifNode->recType == Nif::RC_NiTriShape && !skipMeshes)
            {
                const Nif::NiTriShape* triShape = static_cast<const Nif::NiTriShape*>(nifNode);
                if (!createSkeleton || triShape->skin.empty())
                    handleTriShape(triShape, transformNode, boundTextures, animflags);
                else
                    handleSkinnedTriShape(triShape, transformNode, boundTextures, animflags);

                if (!nifNode->controller.empty())
                    handleMeshControllers(nifNode, transformNode, boundTextures, animflags);
            }

            if(nifNode->recType == Nif::RC_NiAutoNormalParticles || nifNode->recType == Nif::RC_NiRotatingParticles)
                handleParticleSystem(nifNode, transformNode, animflags, particleflags, rootNode);

            if (!nifNode->controller.empty())
                handleNodeControllers(nifNode, transformNode, animflags);

            // Added last so the changes from KeyframeControllers are taken into account
            if (osgAnimation::Bone* bone = dynamic_cast<osgAnimation::Bone*>(transformNode.get()))
                bone->addUpdateCallback(new UpdateBone);

            const Nif::NiNode *ninode = dynamic_cast<const Nif::NiNode*>(nifNode);
            if(ninode)
            {
                const Nif::NodeList &children = ninode->children;
                for(size_t i = 0;i < children.length();++i)
                {
                    if(!children[i].empty())
                    {
                        handleNode(children[i].getPtr(), transformNode, textureManager,
                                   createSkeleton, boundTextures, animflags, particleflags, skipMeshes, textKeys, rootNode);
                    }
                }
            }

            return transformNode;
        }

        static void handleMeshControllers(const Nif::Node *nifNode, osg::MatrixTransform *transformNode, const std::map<int, int> &boundTextures, int animflags)
        {
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;
                if (ctrl->recType == Nif::RC_NiUVController)
                {
                    const Nif::NiUVController *uvctrl = static_cast<const Nif::NiUVController*>(ctrl.getPtr());
                    std::set<int> texUnits;
                    for (std::map<int, int>::const_iterator it = boundTextures.begin(); it != boundTextures.end(); ++it)
                        texUnits.insert(it->first);

                    osg::ref_ptr<UVController> ctrl = new UVController(uvctrl->data.getPtr(), texUnits);
                    setupController(uvctrl, ctrl, animflags);
                    transformNode->getOrCreateStateSet()->setDataVariance(osg::StateSet::DYNAMIC);

                    transformNode->addUpdateCallback(ctrl);
                }
            }
        }

        static void handleNodeControllers(const Nif::Node* nifNode, osg::MatrixTransform* transformNode, int animflags)
        {
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;
                if (ctrl->recType == Nif::RC_NiKeyframeController)
                {
                    const Nif::NiKeyframeController *key = static_cast<const Nif::NiKeyframeController*>(ctrl.getPtr());
                    if(!key->data.empty())
                    {
                        osg::ref_ptr<KeyframeController> callback(new KeyframeController(key->data.getPtr()));

                        setupController(key, callback, animflags);
                        transformNode->addUpdateCallback(callback);
                    }
                }
                else if (ctrl->recType == Nif::RC_NiVisController)
                {
                    const Nif::NiVisController* visctrl = static_cast<const Nif::NiVisController*>(ctrl.getPtr());

                    osg::ref_ptr<VisController> callback(new VisController(visctrl->data.getPtr()));
                    setupController(visctrl, callback, animflags);
                    transformNode->addUpdateCallback(callback);
                }
            }
        }

        static void handleMaterialControllers(const Nif::Property *materialProperty, osg::Node* node, osg::StateSet *stateset, int animflags)
        {
            for (Nif::ControllerPtr ctrl = materialProperty->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;
                if (ctrl->recType == Nif::RC_NiAlphaController)
                {
                    const Nif::NiAlphaController* alphactrl = static_cast<const Nif::NiAlphaController*>(ctrl.getPtr());
                    osg::ref_ptr<AlphaController> ctrl(new AlphaController(alphactrl->data.getPtr()));
                    setupController(alphactrl, ctrl, animflags);
                    stateset->setDataVariance(osg::StateSet::DYNAMIC);
                    node->addUpdateCallback(ctrl);
                }
                else if (ctrl->recType == Nif::RC_NiMaterialColorController)
                {
                    const Nif::NiMaterialColorController* matctrl = static_cast<const Nif::NiMaterialColorController*>(ctrl.getPtr());
                    osg::ref_ptr<MaterialColorController> ctrl(new MaterialColorController(matctrl->data.getPtr()));
                    setupController(matctrl, ctrl, animflags);
                    stateset->setDataVariance(osg::StateSet::DYNAMIC);
                    node->addUpdateCallback(ctrl);
                }
                else
                    std::cerr << "Unexpected material controller " << ctrl->recType << std::endl;
            }
        }

        static void handleTextureControllers(const Nif::Property *texProperty, osg::Node* node, Resource::TextureManager* textureManager, osg::StateSet *stateset, int animflags)
        {
            for (Nif::ControllerPtr ctrl = texProperty->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;
                if (ctrl->recType == Nif::RC_NiFlipController)
                {
                    const Nif::NiFlipController* flipctrl = static_cast<const Nif::NiFlipController*>(ctrl.getPtr());
                    std::vector<osg::ref_ptr<osg::Texture2D> > textures;
                    for (unsigned int i=0; i<flipctrl->mSources.length(); ++i)
                    {
                        Nif::NiSourceTexturePtr st = flipctrl->mSources[i];
                        if (st.empty())
                            continue;

                        // inherit wrap settings from the target slot
                        osg::Texture2D* inherit = dynamic_cast<osg::Texture2D*>(stateset->getTextureAttribute(flipctrl->mTexSlot, osg::StateAttribute::TEXTURE));
                        osg::Texture2D::WrapMode wrapS = osg::Texture2D::CLAMP;
                        osg::Texture2D::WrapMode wrapT = osg::Texture2D::CLAMP;
                        if (inherit)
                        {
                            wrapS = inherit->getWrap(osg::Texture2D::WRAP_S);
                            wrapT = inherit->getWrap(osg::Texture2D::WRAP_T);
                        }

                        std::string filename = Misc::ResourceHelpers::correctTexturePath(st->filename, textureManager->getVFS());
                        osg::ref_ptr<osg::Texture2D> texture = textureManager->getTexture2D(filename, wrapS, wrapT);
                        textures.push_back(texture);
                    }
                    osg::ref_ptr<FlipController> callback(new FlipController(flipctrl, textures));
                    setupController(ctrl.getPtr(), callback, animflags);
                    stateset->setDataVariance(osg::StateSet::DYNAMIC);
                    node->addUpdateCallback(callback);
                }
                else
                    std::cerr << "Unexpected texture controller " << ctrl->recName << std::endl;
            }
        }

        static void handleParticlePrograms(Nif::ExtraPtr affectors, Nif::ExtraPtr colliders, osg::Group *attachTo, osgParticle::ParticleSystem* partsys, osgParticle::ParticleProcessor::ReferenceFrame rf)
        {
            osgParticle::ModularProgram* program = new osgParticle::ModularProgram;
            attachTo->addChild(program);
            program->setParticleSystem(partsys);
            program->setReferenceFrame(rf);
            program->setCullingActive(true);
            for (; !affectors.empty(); affectors = affectors->extra)
            {
                if (affectors->recType == Nif::RC_NiParticleGrowFade)
                {
                    const Nif::NiParticleGrowFade *gf = static_cast<const Nif::NiParticleGrowFade*>(affectors.getPtr());
                    program->addOperator(new GrowFadeAffector(gf->growTime, gf->fadeTime));
                }
                else if (affectors->recType == Nif::RC_NiGravity)
                {
                    const Nif::NiGravity* gr = static_cast<const Nif::NiGravity*>(affectors.getPtr());
                    program->addOperator(new GravityAffector(gr));
                }
                else if (affectors->recType == Nif::RC_NiParticleColorModifier)
                {
                    const Nif::NiParticleColorModifier *cl = static_cast<const Nif::NiParticleColorModifier*>(affectors.getPtr());
                    const Nif::NiColorData *clrdata = cl->data.getPtr();
                    program->addOperator(new ParticleColorAffector(clrdata));
                }
                else if (affectors->recType == Nif::RC_NiParticleRotation)
                {
                    // unused
                }
                else
                    std::cerr << "Unhandled particle modifier " << affectors->recName << std::endl;
            }
            for (; !colliders.empty(); colliders = colliders->extra)
            {
                if (colliders->recType == Nif::RC_NiPlanarCollider)
                {
                    const Nif::NiPlanarCollider* planarcollider = static_cast<const Nif::NiPlanarCollider*>(colliders.getPtr());
                    program->addOperator(new PlanarCollider(planarcollider));
                }
            }
        }

        // Load the initial state of the particle system, i.e. the initial particles and their positions, velocity and colors.
        static void handleParticleInitialState(const Nif::Node* nifNode, osgParticle::ParticleSystem* partsys, const Nif::NiParticleSystemController* partctrl)
        {
            const Nif::NiAutoNormalParticlesData *particledata = NULL;
            if(nifNode->recType == Nif::RC_NiAutoNormalParticles)
                particledata = static_cast<const Nif::NiAutoNormalParticles*>(nifNode)->data.getPtr();
            else if(nifNode->recType == Nif::RC_NiRotatingParticles)
                particledata = static_cast<const Nif::NiRotatingParticles*>(nifNode)->data.getPtr();
            else
                return;

            int i=0;
            for (std::vector<Nif::NiParticleSystemController::Particle>::const_iterator it = partctrl->particles.begin();
                 i<particledata->activeCount && it != partctrl->particles.end(); ++it, ++i)
            {
                const Nif::NiParticleSystemController::Particle& particle = *it;

                ParticleAgeSetter particletemplate(std::max(0.f, particle.lifetime));

                osgParticle::Particle* created = partsys->createParticle(&particletemplate);
                created->setLifeTime(std::max(0.f, particle.lifespan));

                // Note this position and velocity is not correct for a particle system with absolute reference frame,
                // which can not be done in this loader since we are not attached to the scene yet. Will be fixed up post-load in the SceneManager.
                created->setVelocity(particle.velocity);
                created->setPosition(particledata->vertices.at(particle.vertex));

                osg::Vec4f partcolor (1.f,1.f,1.f,1.f);
                if (particle.vertex < int(particledata->colors.size()))
                    partcolor = particledata->colors.at(particle.vertex);

                float size = particledata->sizes.at(particle.vertex) * partctrl->size;

                created->setSizeRange(osgParticle::rangef(size, size));
            }
        }

        static osg::ref_ptr<Emitter> handleParticleEmitter(const Nif::NiParticleSystemController* partctrl)
        {
            std::vector<int> targets;
            if (partctrl->recType == Nif::RC_NiBSPArrayController)
            {
                getAllNiNodes(partctrl->emitter.getPtr(), targets);
            }

            osg::ref_ptr<Emitter> emitter = new Emitter(targets);

            osgParticle::ConstantRateCounter* counter = new osgParticle::ConstantRateCounter;
            if (partctrl->emitFlags & Nif::NiParticleSystemController::NoAutoAdjust)
                counter->setNumberOfParticlesPerSecondToCreate(partctrl->emitRate);
            else
                counter->setNumberOfParticlesPerSecondToCreate(partctrl->numParticles / (partctrl->lifetime + partctrl->lifetimeRandom/2));

            emitter->setCounter(counter);

            ParticleShooter* shooter = new ParticleShooter(partctrl->velocity - partctrl->velocityRandom*0.5f,
                                                           partctrl->velocity + partctrl->velocityRandom*0.5f,
                                                           partctrl->horizontalDir, partctrl->horizontalAngle,
                                                           partctrl->verticalDir, partctrl->verticalAngle,
                                                           partctrl->lifetime, partctrl->lifetimeRandom);
            emitter->setShooter(shooter);

            osgParticle::BoxPlacer* placer = new osgParticle::BoxPlacer;
            placer->setXRange(-partctrl->offsetRandom.x(), partctrl->offsetRandom.x());
            placer->setYRange(-partctrl->offsetRandom.y(), partctrl->offsetRandom.y());
            placer->setZRange(-partctrl->offsetRandom.z(), partctrl->offsetRandom.z());

            emitter->setPlacer(placer);
            return emitter;
        }

        static void handleParticleSystem(const Nif::Node *nifNode, osg::Group *parentNode, int animflags, int particleflags, osg::Node* rootNode)
        {
            osg::ref_ptr<ParticleSystem> partsys (new ParticleSystem);
            partsys->setSortMode(osgParticle::ParticleSystem::SORT_BACK_TO_FRONT);

            const Nif::NiParticleSystemController* partctrl = NULL;
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!(ctrl->flags & Nif::NiNode::ControllerFlag_Active))
                    continue;
                if(ctrl->recType == Nif::RC_NiParticleSystemController || ctrl->recType == Nif::RC_NiBSPArrayController)
                    partctrl = static_cast<Nif::NiParticleSystemController*>(ctrl.getPtr());
            }
            if (!partctrl)
            {
                std::cerr << "No particle controller found " << std::endl;
                return;
            }

            osgParticle::ParticleProcessor::ReferenceFrame rf = (particleflags & Nif::NiNode::ParticleFlag_LocalSpace)
                    ? osgParticle::ParticleProcessor::RELATIVE_RF
                    : osgParticle::ParticleProcessor::ABSOLUTE_RF;

            // HACK: ParticleSystem has no setReferenceFrame method
            if (rf == osgParticle::ParticleProcessor::ABSOLUTE_RF)
            {
                partsys->getOrCreateUserDataContainer()->addDescription("worldspace");
            }

            handleParticleInitialState(nifNode, partsys, partctrl);

            partsys->setQuota(partctrl->numParticles);

            partsys->getDefaultParticleTemplate().setSizeRange(osgParticle::rangef(partctrl->size, partctrl->size));
            partsys->getDefaultParticleTemplate().setColorRange(osgParticle::rangev4(osg::Vec4f(1.f,1.f,1.f,1.f), osg::Vec4f(1.f,1.f,1.f,1.f)));
            partsys->getDefaultParticleTemplate().setAlphaRange(osgParticle::rangef(1.f, 1.f));

            osg::ref_ptr<Emitter> emitter = handleParticleEmitter(partctrl);
            emitter->setParticleSystem(partsys);
            emitter->setReferenceFrame(osgParticle::ParticleProcessor::RELATIVE_RF);
            emitter->setCullingActive(true);

            // Note: we assume that the Emitter node is placed *before* the Particle node in the scene graph.
            // This seems to be true for all NIF files in the game that I've checked, suggesting that NIFs work similar to OSG with regards to update order.
            // If something ever violates this assumption, the worst that could happen is the culling being one frame late, which wouldn't be a disaster.

            FindRecIndexVisitor find (partctrl->emitter->recIndex);
            rootNode->accept(find);
            if (!find.mFound)
            {
                std::cerr << "can't find emitter node, wrong node order?" << std::endl;
                return;
            }
            osg::Group* emitterNode = find.mFound;

            // Emitter attached to the emitter node. Note one side effect of the emitter using the CullVisitor is that hiding its node
            // actually causes the emitter to stop firing. Convenient, because MW behaves this way too!
            emitterNode->addChild(emitter);

            osg::ref_ptr<ParticleSystemController> callback(new ParticleSystemController(partctrl));
            setupController(partctrl, callback, animflags);
            emitter->setUpdateCallback(callback);

            // affectors must be attached *after* the emitter in the scene graph for correct update order
            // attach to same node as the ParticleSystem, we need osgParticle Operators to get the correct
            // localToWorldMatrix for transforming to particle space
            handleParticlePrograms(partctrl->affectors, partctrl->colliders, parentNode, partsys.get(), rf);

            osg::ref_ptr<osg::Geode> geode (new osg::Geode);
            geode->addDrawable(partsys);

            std::vector<const Nif::Property*> materialProps;
            collectMaterialProperties(nifNode, materialProps);
            applyMaterialProperties(geode, materialProps, true, animflags);

            partsys->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
            partsys->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

            // particle system updater (after the emitters and affectors in the scene graph)
            // I think for correct culling needs to be *before* the ParticleSystem, though osg examples do it the other way
            osg::ref_ptr<osgParticle::ParticleSystemUpdater> updater = new osgParticle::ParticleSystemUpdater;
            updater->setCullingActive(true);
            updater->addParticleSystem(partsys);
            parentNode->addChild(updater);

            if (rf == osgParticle::ParticleProcessor::RELATIVE_RF)
                parentNode->addChild(geode);
            else
            {
                osg::MatrixTransform* trans = new osg::MatrixTransform;
                trans->setUpdateCallback(new InverseWorldMatrix);
                trans->addChild(geode);
                parentNode->addChild(trans);
            }
        }

        static void triShapeToGeometry(const Nif::NiTriShape *triShape, osg::Geometry *geometry, osg::Geode* parentGeode, const std::map<int, int>& boundTextures, int animflags)
        {
            const Nif::NiTriShapeData* data = triShape->data.getPtr();

            const Nif::NiSkinInstance *skin = (triShape->skin.empty() ? NULL : triShape->skin.getPtr());
            if (skin)
            {
                // Convert vertices and normals to bone space from bind position. It would be
                // better to transform the bones into bind position, but there doesn't seem to
                // be a reliable way to do that.
                osg::ref_ptr<osg::Vec3Array> newVerts (new osg::Vec3Array(data->vertices.size()));
                osg::ref_ptr<osg::Vec3Array> newNormals (new osg::Vec3Array(data->normals.size()));

                const Nif::NiSkinData *skinData = skin->data.getPtr();
                const Nif::NodeList &bones = skin->bones;
                for(size_t b = 0;b < bones.length();b++)
                {
                    osg::Matrixf mat = toMatrix(skinData->bones[b].trafo);

                    mat = mat * getWorldTransform(bones[b].getPtr());

                    const std::vector<Nif::NiSkinData::VertWeight> &weights = skinData->bones[b].weights;
                    for(size_t i = 0;i < weights.size();i++)
                    {
                        size_t index = weights[i].vertex;
                        float weight = weights[i].weight;

                        osg::Vec4f mult = (osg::Vec4f(data->vertices.at(index),1.f) * mat) * weight;
                        (*newVerts)[index] += osg::Vec3f(mult.x(),mult.y(),mult.z());
                        if(newNormals->size() > index)
                        {
                            osg::Vec4 normal(data->normals[index].x(), data->normals[index].y(), data->normals[index].z(), 0.f);
                            normal = (normal * mat) * weight;
                            (*newNormals)[index] += osg::Vec3f(normal.x(),normal.y(),normal.z());
                        }
                    }
                }
                // Interpolating normalized normals doesn't necessarily give you a normalized result
                // Currently we're using GL_NORMALIZE, so this isn't needed
                //for (unsigned int i=0;i<newNormals->size();++i)
                //    (*newNormals)[i].normalize();

                geometry->setVertexArray(newVerts);
                if (!data->normals.empty())
                    geometry->setNormalArray(newNormals, osg::Array::BIND_PER_VERTEX);
            }
            else
            {
                geometry->setVertexArray(new osg::Vec3Array(data->vertices.size(), &data->vertices[0]));
                if (!data->normals.empty())
                    geometry->setNormalArray(new osg::Vec3Array(data->normals.size(), &data->normals[0]), osg::Array::BIND_PER_VERTEX);
            }

            for (std::map<int, int>::const_iterator it = boundTextures.begin(); it != boundTextures.end(); ++it)
            {
                int textureStage = it->first;
                int uvSet = it->second;
                if (uvSet >= (int)data->uvlist.size())
                {
                    // Occurred in "ascendedsleeper.nif", but only for hidden Shadow nodes, apparently
                    //std::cerr << "Warning: using an undefined UV set " << uvSet << " on TriShape " << triShape->name << std::endl;
                    continue;
                }

                geometry->setTexCoordArray(textureStage, new osg::Vec2Array(data->uvlist[uvSet].size(), &data->uvlist[uvSet][0]), osg::Array::BIND_PER_VERTEX);
            }

            if (!data->colors.empty())
                geometry->setColorArray(new osg::Vec4Array(data->colors.size(), &data->colors[0]), osg::Array::BIND_PER_VERTEX);

            geometry->addPrimitiveSet(new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES,
                                                                  data->triangles.size(),
                                                                  (unsigned short*)&data->triangles[0]));

            // osg::Material properties are handled here for two reasons:
            // - if there are no vertex colors, we need to disable colorMode.
            // - there are 3 "overlapping" nif properties that all affect the osg::Material, handling them
            //   above the actual renderable would be tedious.
            std::vector<const Nif::Property*> materialProps;
            collectMaterialProperties(triShape, materialProps);
            applyMaterialProperties(parentGeode, materialProps, !data->colors.empty(), animflags);
        }

        static void handleTriShape(const Nif::NiTriShape* triShape, osg::Group* parentNode, const std::map<int, int>& boundTextures, int animflags)
        {
            osg::ref_ptr<osg::Geometry> geometry;
            if(!triShape->controller.empty())
            {
                Nif::ControllerPtr ctrl = triShape->controller;
                do {
                    if(ctrl->recType == Nif::RC_NiGeomMorpherController && ctrl->flags & Nif::NiNode::ControllerFlag_Active)
                    {
                        geometry = handleMorphGeometry(static_cast<const Nif::NiGeomMorpherController*>(ctrl.getPtr()));

                        osg::ref_ptr<GeomMorpherController> morphctrl = new GeomMorpherController(
                                    static_cast<const Nif::NiGeomMorpherController*>(ctrl.getPtr())->data.getPtr());
                        setupController(ctrl.getPtr(), morphctrl, animflags);
                        geometry->setUpdateCallback(morphctrl);
                        break;
                    }
                } while(!(ctrl=ctrl->next).empty());
            }

            if (!geometry.get())
                geometry = new osg::Geometry;

            osg::ref_ptr<osg::Geode> geode (new osg::Geode);
            geode->setName(triShape->name); // name will be used for part filtering
            triShapeToGeometry(triShape, geometry, geode, boundTextures, animflags);

            geode->addDrawable(geometry);

            parentNode->addChild(geode);
        }

        static osg::ref_ptr<osg::Geometry> handleMorphGeometry(const Nif::NiGeomMorpherController* morpher)
        {
            osg::ref_ptr<osgAnimation::MorphGeometry> morphGeom = new osgAnimation::MorphGeometry;
            morphGeom->setMethod(osgAnimation::MorphGeometry::RELATIVE);
            // No normals available in the MorphData
            morphGeom->setMorphNormals(false);

            const std::vector<Nif::NiMorphData::MorphData>& morphs = morpher->data.getPtr()->mMorphs;
            // Note we are not interested in morph 0, which just contains the original vertices
            for (unsigned int i = 1; i < morphs.size(); ++i)
            {
                osg::ref_ptr<osg::Geometry> morphTarget = new osg::Geometry;
                morphTarget->setVertexArray(new osg::Vec3Array(morphs[i].mVertices.size(), &morphs[i].mVertices[0]));
                morphGeom->addMorphTarget(morphTarget, 0.f);
            }
            return morphGeom;
        }

        static void handleSkinnedTriShape(const Nif::NiTriShape *triShape, osg::Group *parentNode, const std::map<int, int>& boundTextures, int animflags)
        {
            osg::ref_ptr<osg::Geode> geode (new osg::Geode);
            geode->setName(triShape->name); // name will be used for part filtering
            osg::ref_ptr<osg::Geometry> geometry (new osg::Geometry);
            triShapeToGeometry(triShape, geometry, geode, boundTextures, animflags);

            // Note the RigGeometry's UpdateCallback uses the skeleton space bone matrix, so the bone UpdateCallback has to be fired first.
            // For this to work properly, all bones used for skinning a RigGeometry need to be created before that RigGeometry.
            // All NIFs I've checked seem to conform to this restriction, perhaps Gamebryo update method works similarly.
            // If a file violates this assumption, the worst that could happen is the bone position being a frame late.
            // If this happens, we should get a warning from the Skeleton's validation update callback on the error log.
            osg::ref_ptr<osgAnimation::RigGeometry> rig(new osgAnimation::RigGeometry);
            rig->setSourceGeometry(geometry);

            const Nif::NiSkinInstance *skin = triShape->skin.getPtr();

            RigBoundingBoxCallback* callback = new RigBoundingBoxCallback;
            rig->setComputeBoundingBoxCallback(callback);

            // Assign bone weights
            osg::ref_ptr<osgAnimation::VertexInfluenceMap> map (new osgAnimation::VertexInfluenceMap);

            const Nif::NiSkinData *data = skin->data.getPtr();
            const Nif::NodeList &bones = skin->bones;
            for(size_t i = 0;i < bones.length();i++)
            {
                std::string boneName = bones[i].getPtr()->name;

                callback->addBoundSphere(boneName, osg::BoundingSphere(data->bones[i].boundSphereCenter, data->bones[i].boundSphereRadius));

                osgAnimation::VertexInfluence influence;
                influence.setName(boneName);
                const std::vector<Nif::NiSkinData::VertWeight> &weights = data->bones[i].weights;
                influence.reserve(weights.size());
                for(size_t j = 0;j < weights.size();j++)
                {
                    osgAnimation::VertexIndexWeight indexWeight = std::make_pair(weights[j].vertex, weights[j].weight);
                    influence.push_back(indexWeight);
                }

                map->insert(std::make_pair(boneName, influence));
            }
            rig->setInfluenceMap(map);

            // Override default update using cull callback instead for efficiency.
            rig->setUpdateCallback(NULL);
            rig->setCullCallback(new UpdateRigGeometry);

            osg::ref_ptr<osg::MatrixTransform> trans(new osg::MatrixTransform);
            trans->setUpdateCallback(new InvertBoneMatrix());

            geode->addDrawable(rig);
            geode->addUpdateCallback(new DirtyBoundCallback);

            trans->addChild(geode);
            parentNode->addChild(trans);
        }


        static void handleProperty(const Nif::Property *property,
                            osg::Node *node, Resource::TextureManager* textureManager, std::map<int, int>& boundTextures, int animflags)
        {
            osg::StateSet* stateset = node->getOrCreateStateSet();

            switch (property->recType)
            {
            case Nif::RC_NiStencilProperty:
            {
                const Nif::NiStencilProperty* stencilprop = static_cast<const Nif::NiStencilProperty*>(property);
                osg::FrontFace* frontFace = new osg::FrontFace;
                switch (stencilprop->data.drawMode)
                {
                case 1:
                    frontFace->setMode(osg::FrontFace::CLOCKWISE);
                    break;
                case 0:
                case 2:
                default:
                    frontFace->setMode(osg::FrontFace::COUNTER_CLOCKWISE);
                    break;
                }

                stateset->setAttribute(frontFace, osg::StateAttribute::ON);
                stateset->setMode(GL_CULL_FACE, stencilprop->data.drawMode == 3 ? osg::StateAttribute::OFF
                                                                                : osg::StateAttribute::ON);

                if (stencilprop->data.enabled != 0)
                {
                    osg::Stencil* stencil = new osg::Stencil;
                    stencil->setFunction(getStencilFunction(stencilprop->data.compareFunc), stencilprop->data.stencilRef, stencilprop->data.stencilMask);
                    stencil->setStencilFailOperation(getStencilOperation(stencilprop->data.failAction));
                    stencil->setStencilPassAndDepthFailOperation(getStencilOperation(stencilprop->data.zFailAction));
                    stencil->setStencilPassAndDepthPassOperation(getStencilOperation(stencilprop->data.zPassAction));

                    stateset->setAttributeAndModes(stencil, osg::StateAttribute::ON);
                }
                break;
            }
            case Nif::RC_NiWireframeProperty:
            {
                const Nif::NiWireframeProperty* wireprop = static_cast<const Nif::NiWireframeProperty*>(property);
                osg::PolygonMode* mode = new osg::PolygonMode;
                mode->setMode(osg::PolygonMode::FRONT_AND_BACK, wireprop->flags == 0 ? osg::PolygonMode::FILL
                                                                                     : osg::PolygonMode::LINE);
                stateset->setAttributeAndModes(mode, osg::StateAttribute::ON);
                break;
            }
            case Nif::RC_NiZBufferProperty:
            {
                const Nif::NiZBufferProperty* zprop = static_cast<const Nif::NiZBufferProperty*>(property);
                // VER_MW doesn't support a DepthFunction according to NifSkope
                osg::Depth* depth = new osg::Depth;
                depth->setWriteMask((zprop->flags>>1)&1);
                stateset->setAttributeAndModes(depth, osg::StateAttribute::ON);
                break;
            }
            // OSG groups the material properties that NIFs have separate, so we have to parse them all again when one changed
            case Nif::RC_NiMaterialProperty:
            case Nif::RC_NiVertexColorProperty:
            case Nif::RC_NiSpecularProperty:
            {
                // Handled in handleTriShape so we know whether vertex colors are available
                break;
            }
            case Nif::RC_NiAlphaProperty:
            {
                const Nif::NiAlphaProperty* alphaprop = static_cast<const Nif::NiAlphaProperty*>(property);
                osg::BlendFunc* blendfunc = new osg::BlendFunc;
                if (alphaprop->flags&1)
                {
                    blendfunc->setFunction(getBlendMode((alphaprop->flags>>1)&0xf),
                                           getBlendMode((alphaprop->flags>>5)&0xf));
                    stateset->setAttributeAndModes(blendfunc, osg::StateAttribute::ON);

                    bool noSort = (alphaprop->flags>>13)&1;
                    if (!noSort)
                    {
                        stateset->setNestRenderBins(false);
                        stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                    }
                }
                else
                {
                    stateset->setAttributeAndModes(blendfunc, osg::StateAttribute::OFF);
                    stateset->setNestRenderBins(false);
                    stateset->setRenderingHint(osg::StateSet::OPAQUE_BIN);
                }

                osg::AlphaFunc* alphafunc = new osg::AlphaFunc;
                if((alphaprop->flags>>9)&1)
                {
                    alphafunc->setFunction(getTestMode((alphaprop->flags>>10)&0x7), alphaprop->data.threshold/255.f);
                    stateset->setAttributeAndModes(alphafunc, osg::StateAttribute::ON);
                }
                else
                    stateset->setAttributeAndModes(alphafunc, osg::StateAttribute::OFF);
                break;
            }
            case Nif::RC_NiTexturingProperty:
            {
                const Nif::NiTexturingProperty* texprop = static_cast<const Nif::NiTexturingProperty*>(property);
                for (int i=0; i<Nif::NiTexturingProperty::NumTextures; ++i)
                {
                    if (texprop->textures[i].inUse)
                    {
                        if (i != Nif::NiTexturingProperty::BaseTexture
                                && i != Nif::NiTexturingProperty::GlowTexture
                                && i != Nif::NiTexturingProperty::DarkTexture
                                && i != Nif::NiTexturingProperty::DetailTexture)
                        {
                            std::cerr << "Warning: unhandled texture stage " << i << std::endl;
                            continue;
                        }

                        const Nif::NiTexturingProperty::Texture& tex = texprop->textures[i];
                        if(tex.texture.empty())
                        {
                            std::cerr << "Warning: texture layer " << i << " is in use but empty " << std::endl;
                            continue;
                        }
                        const Nif::NiSourceTexture *st = tex.texture.getPtr();
                        if (!st->external)
                        {
                            std::cerr << "Warning: unhandled internal texture " << std::endl;
                            continue;
                        }

                        std::string filename = Misc::ResourceHelpers::correctTexturePath(st->filename, textureManager->getVFS());

                        unsigned int clamp = static_cast<unsigned int>(tex.clamp);
                        int wrapT = (clamp) & 0x1;
                        int wrapS = (clamp >> 1) & 0x1;

                        osg::Texture2D* texture2d = textureManager->getTexture2D(filename,
                              wrapS ? osg::Texture::REPEAT : osg::Texture::CLAMP,
                              wrapT ? osg::Texture::REPEAT : osg::Texture::CLAMP);

                        stateset->setTextureAttributeAndModes(i, texture2d, osg::StateAttribute::ON);

                        if (i == Nif::NiTexturingProperty::GlowTexture)
                        {
                            osg::TexEnv* texEnv = new osg::TexEnv;
                            texEnv->setMode(osg::TexEnv::ADD);
                            stateset->setTextureAttributeAndModes(i, texEnv, osg::StateAttribute::ON);
                        }
                        else if (i == Nif::NiTexturingProperty::DarkTexture)
                        {
                            osg::TexEnv* texEnv = new osg::TexEnv;
                            texEnv->setMode(osg::TexEnv::MODULATE);
                            stateset->setTextureAttributeAndModes(i, texEnv, osg::StateAttribute::ON);
                        }
                        else if (i == Nif::NiTexturingProperty::DetailTexture)
                        {
                            osg::TexEnvCombine* texEnv = new osg::TexEnvCombine;
                            texEnv->setScale_RGB(2.f);
                            texEnv->setCombine_Alpha(GL_MODULATE);
                            texEnv->setOperand0_Alpha(GL_SRC_ALPHA);
                            texEnv->setOperand1_Alpha(GL_SRC_ALPHA);
                            texEnv->setSource0_Alpha(GL_PREVIOUS);
                            texEnv->setSource1_Alpha(GL_TEXTURE);
                            texEnv->setCombine_RGB(GL_MODULATE);
                            texEnv->setOperand0_RGB(GL_SRC_COLOR);
                            texEnv->setOperand1_RGB(GL_SRC_COLOR);
                            texEnv->setSource0_RGB(GL_PREVIOUS);
                            texEnv->setSource1_RGB(GL_TEXTURE);
                            stateset->setTextureAttributeAndModes(i, texEnv, osg::StateAttribute::ON);
                        }

                        boundTextures[i] = tex.uvSet;
                    }
                    else if (boundTextures.find(i) != boundTextures.end())
                    {
                        stateset->setTextureAttributeAndModes(i, new osg::Texture2D, osg::StateAttribute::OFF);
                        boundTextures.erase(i);
                    }
                    handleTextureControllers(texprop, node, textureManager, stateset, animflags);
                }
                break;
            }
            // unused by mw
            case Nif::RC_NiShadeProperty:
            case Nif::RC_NiDitherProperty:
            case Nif::RC_NiFogProperty:
            {
                break;
            }
            default:
                std::cerr << "Unhandled " << property->recName << std::endl;
                break;
            }
        }

        static void applyMaterialProperties(osg::Node* node, const std::vector<const Nif::Property*>& properties,
                                             bool hasVertexColors, int animflags)
        {
            osg::StateSet* stateset = node->getOrCreateStateSet();

            int specFlags = 0; // Specular is disabled by default, even if there's a specular color in the NiMaterialProperty
            osg::Material* mat = new osg::Material;
            mat->setColorMode(hasVertexColors ? osg::Material::AMBIENT_AND_DIFFUSE : osg::Material::OFF);

            // NIF material defaults don't match OpenGL defaults
            mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(1,1,1,1));
            mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1,1,1,1));

            for (std::vector<const Nif::Property*>::const_reverse_iterator it = properties.rbegin(); it != properties.rend(); ++it)
            {
                const Nif::Property* property = *it;
                switch (property->recType)
                {
                case Nif::RC_NiSpecularProperty:
                {
                    specFlags = property->flags;
                    break;
                }
                case Nif::RC_NiMaterialProperty:
                {
                    const Nif::NiMaterialProperty* matprop = static_cast<const Nif::NiMaterialProperty*>(property);

                    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.diffuse, matprop->data.alpha));
                    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.ambient, 1.f));
                    mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.emissive, 1.f));

                    mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.specular, 1.f));
                    mat->setShininess(osg::Material::FRONT_AND_BACK, matprop->data.glossiness);

                    if (!matprop->controller.empty())
                        handleMaterialControllers(matprop, node, stateset, animflags);

                    break;
                }
                case Nif::RC_NiVertexColorProperty:
                {
                    const Nif::NiVertexColorProperty* vertprop = static_cast<const Nif::NiVertexColorProperty*>(property);
                    if (!hasVertexColors)
                        break;
                    switch (vertprop->flags)
                    {
                    case 0:
                        mat->setColorMode(osg::Material::OFF);
                        break;
                    case 1:
                        mat->setColorMode(osg::Material::EMISSION);
                        break;
                    case 2:
                        mat->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
                        break;
                    }
                }
                }
            }

            if (specFlags == 0)
                mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f,0.f,0.f,0.f));

            stateset->setAttributeAndModes(mat, osg::StateAttribute::ON);
        }

    };

    osg::ref_ptr<osg::Node> Loader::load(Nif::NIFFilePtr file, Resource::TextureManager* textureManager)
    {
        return LoaderImpl::load(file, textureManager);
    }

    osg::ref_ptr<osg::Node> Loader::loadAsSkeleton(Nif::NIFFilePtr file, Resource::TextureManager* textureManager)
    {
        return LoaderImpl::loadAsSkeleton(file, textureManager);
    }

    void Loader::loadKf(Nif::NIFFilePtr kf, KeyframeHolder& target)
    {
        LoaderImpl::loadKf(kf, target);
    }

}