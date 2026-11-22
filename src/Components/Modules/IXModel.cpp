#include "STDInclude.hpp"

using namespace std::literals;

static_assert(sizeof(Game::IW3::XModel) == 220, "Size of XModel is invalid!");

namespace Components
{
	std::unordered_map<IXModel::IW3ModelSurf, IXModel::IW4ModelSurfCouple, IXModel::IW3ModelSurfHasher> IXModel::convertedModelSurfaces;

	Game::IW4::XModel* IXModel::Convert(Game::IW3::XModel* model)
	{
		if (!model) return nullptr;

		Game::IW4::XModel xmodel;
		ZeroMemory(&xmodel, sizeof(xmodel));

		xmodel.name = model->name;
		xmodel.numBones = model->numBones;
		xmodel.numRootBones = model->numRootBones;
		xmodel.numsurfs = model->numsurfs;
		xmodel.lodRampType = model->lodRampType;

		xmodel.scale = 1.0f;

		xmodel.boneNames = model->boneNames;
		xmodel.parentList = model->parentList;
		xmodel.quats = model->quats;
		xmodel.trans = model->trans;
		xmodel.partClassification = model->partClassification;
		xmodel.baseMat = model->baseMat;

		xmodel.materialHandles = LocalAllocator.AllocateArray<Game::IW4::Material*>(model->numsurfs);
		for (size_t i = 0; i < model->numsurfs; i++)
		{
			xmodel.materialHandles[i] = AssetHandler::Convert(Game::IW3::ASSET_TYPE_MATERIAL, { model->materialHandles[i] }).material;
		}

		for (uint8_t i = 0; i < model->numLods; ++i)
		{
			if (MapDumper::ShouldExtendCulling())
			{
				xmodel.lodInfo[i].dist = model->lodInfo[i].dist * 1.5f; // LOD distance is increased so that the maps look nicer in iw4
			}
			else
			{
				xmodel.lodInfo[i].dist = model->lodInfo[i].dist;
			}

			xmodel.lodInfo[i].numsurfs = model->lodInfo[i].numsurfs;
			xmodel.lodInfo[i].surfIndex = model->lodInfo[i].surfIndex;

			// 6 vs 4 part bit elements
			std::memcpy(xmodel.lodInfo[i].partBits, model->lodInfo[i].partBits, sizeof(model->lodInfo[i].partBits));

			xmodel.lodInfo[i].lod = model->lodInfo[i].lod;

#if 0
			// Not correct
			xmodel.lodInfo[i].smcBaseIndexPlusOne = model->lodInfo[i].smcIndexPlusOne;
			xmodel.lodInfo[i].smcSubIndexMask = model->lodInfo[i].smcAllocBits;
			xmodel.lodInfo[i].smcBucket = model->lodInfo[i].unused;
#else

			static const auto useIW4SMC = Game::Dvar_FindVar("iw3x_fix_model_cache");
			static const bool shouldRepairCache = useIW4SMC && useIW4SMC->current.string == "1"s;

			if (model->lodInfo[i].smcIndexPlusOne)
			{
				if (shouldRepairCache)
				{
					// smcIndex is always between 0 and 5 (included).
					// On iw3 it's always between 0 and 3
					// We can use it directly for mw2
					xmodel.lodInfo[i].smcBaseIndexPlusOne = model->lodInfo[i].smcIndexPlusOne;

					// smcAllocBits is always between 9 and 0 (included). It will use some of the available the buckets of mw2
					// 9 (512) = bucket 22
					// 8 (256) = 18
					// 7 (128) = 13
					// 6 (64) = 8
					// 5 (32) = 3
					// 4 (16) = 0
					// 3 and under ??
					char buckets[] = { 0, 0, 0, 0, 0, 3, 8, 13, 18, 22 };
					int allocBits = static_cast<int>(model->lodInfo[i].smcAllocBits);
					assert(allocBits >= 0);
					assert(allocBits < ARRAYSIZE(buckets));
					xmodel.lodInfo[i].smcBucket = buckets[allocBits];

					xmodel.lodInfo[i].smcSubIndexMask = 0; // As far as I know, this one is unused

					if (xmodel.lodInfo[i].smcBucket > 24 || xmodel.lodInfo[i].smcBucket < 0)
					{
						// Impossible
						xmodel.lodInfo[i].smcBaseIndexPlusOne = 0;
						xmodel.lodInfo[i].smcBucket = 0;
						assert(false && "unreachable");
					}
				}
			}
			else
			{
				assert(model->lodInfo[i].smcAllocBits == 0);

				xmodel.lodInfo[i].smcBaseIndexPlusOne = 0;
				xmodel.lodInfo[i].smcSubIndexMask = 0;
				xmodel.lodInfo[i].smcBucket = 0;
			}
#endif

			if (xmodel.lodInfo[i].numsurfs)
			{
				xmodel.lodInfo[i].surfs = LocalAllocator.AllocateArray<Game::IW4::XSurface>(xmodel.lodInfo[i].numsurfs);

				for (unsigned __int16 j = 0; j < xmodel.lodInfo[i].numsurfs; ++j)
				{
					Game::IW4::XSurface* target = &xmodel.lodInfo[i].surfs[j];
					Game::IW3::XSurface* source = &model->surfs[j + xmodel.lodInfo[i].surfIndex];

					target->tileMode = source->tileMode;
					target->deformed = source->deformed;
					target->vertCount = source->vertCount;
					target->triCount = source->triCount;
					target->zoneHandle = source->zoneHandle;
					target->baseTriIndex = source->baseTriIndex;
					target->baseVertIndex = source->baseVertIndex;
					target->triIndices = source->triIndices;
					target->vertInfo = source->vertInfo;
					target->verts0 = source->verts0;
					target->vertListCount = source->vertListCount;
					target->vertList = source->vertList;

					if (i != xmodel.collLod)
					{
						for (size_t k = 0; k < target->vertListCount; k++)
						{
							target->vertList[k].collisionTree = nullptr; // Only collod is used
						}
					}

					// 6 vs 4 part bit elements
					std::memcpy(target->partBits, source->partBits, sizeof(source->partBits));
				}


				IW3ModelSurf surf(&model->surfs[xmodel.lodInfo[i].surfIndex], static_cast<std::uint8_t>(model->lodInfo[i].numsurfs));

				if (convertedModelSurfaces.contains(surf))
				{
					const auto iw4Couple = convertedModelSurfaces[surf];
					xmodel.lodInfo[i].modelSurfs = iw4Couple.xModelSurfs;
					xmodel.lodInfo[i].surfs = iw4Couple.surfs;
				}
				else
				{
					xmodel.lodInfo[i].modelSurfs = LocalAllocator.Allocate<Game::IW4::XModelSurfs>();
					xmodel.lodInfo[i].modelSurfs->name = LocalAllocator.DuplicateString(Utils::VA("%s_lod%d", model->name, i & 0xFF));
					xmodel.lodInfo[i].modelSurfs->numSurfaces = static_cast<int>(xmodel.lodInfo[i].numsurfs);
					xmodel.lodInfo[i].modelSurfs->surfaces = xmodel.lodInfo[i].surfs;

					convertedModelSurfaces[surf] = IW4ModelSurfCouple(xmodel.lodInfo[i].modelSurfs->surfaces, xmodel.lodInfo[i].modelSurfs);
				}

				// 6 vs 4 part bit elements
				std::memcpy(xmodel.lodInfo[i].modelSurfs->partBits, model->lodInfo[i].partBits, sizeof(model->lodInfo[i].partBits));
			}
		}

		xmodel.numLods = static_cast<char>(model->numLods);
		xmodel.collLod = static_cast<char>(model->collLod);
		xmodel.flags = model->flags;

		if (model->collSurfs)
		{
			xmodel.collSurfs = LocalAllocator.AllocateArray<Game::IW4::XModelCollSurf_s>(model->numCollSurfs);
			for (int i = 0; i < model->numCollSurfs; ++i)
			{
				static_assert(sizeof(Game::IW3::XModelCollSurf_s) == sizeof(Game::IW4::XModelCollSurf_s), "Size mismatch");
				std::memcpy(&xmodel.collSurfs[i], &model->collSurfs[i], sizeof(Game::IW4::XModelCollSurf_s));
				xmodel.collSurfs[i].bounds.compute(model->collSurfs[i].mins, model->collSurfs[i].maxs);
			}
		}

		xmodel.numCollSurfs = model->numCollSurfs;
		xmodel.contents = model->contents;

		if (model->boneInfo)
		{
			xmodel.boneInfo = LocalAllocator.AllocateArray<Game::IW4::XBoneInfo>(model->numBones);

			for (char i = 0; i < model->numBones; ++i)
			{
				Game::IW4::XBoneInfo* target = &xmodel.boneInfo[i];
				Game::IW3::XBoneInfo* source = &model->boneInfo[i];

				target->radiusSquared = source->radiusSquared;

				target->bounds.compute(source->bounds[0], source->bounds[1]);
				target->bounds.midPoint[0] += source->offset[0];
				target->bounds.midPoint[1] += source->offset[1];
				target->bounds.midPoint[2] += source->offset[2];
			}
		}

		xmodel.radius = model->radius;

		xmodel.bounds.compute(model->mins, model->maxs);

		xmodel.memUsage = model->memUsage;
		xmodel.bad = model->bad;
		xmodel.physPreset = model->physPreset;

		if (model->physGeoms)
		{
			xmodel.physCollmap = LocalAllocator.Allocate<Game::IW4::PhysCollmap>();
			xmodel.physCollmap->name = LocalAllocator.DuplicateString(Utils::VA("%s_colmap", model->name));
			xmodel.physCollmap->count = model->physGeoms->count;
			xmodel.physCollmap->mass = model->physGeoms->mass;
			xmodel.physCollmap->bounds = xmodel.bounds; // it's fine right?
			xmodel.physCollmap->geoms = LocalAllocator.AllocateArray<Game::IW4::PhysGeomInfo>(model->physGeoms->count);

			for (unsigned int i = 0; i < model->physGeoms->count; ++i)
			{
				static_assert(sizeof(Game::IW4::PhysGeomInfo) == sizeof(Game::IW3::PhysGeomInfo), "Size mismatch");
				Game::IW4::PhysGeomInfo* target = &xmodel.physCollmap->geoms[i];
				Game::IW3::PhysGeomInfo* source = &model->physGeoms->geoms[i];

				std::memcpy(target, source, sizeof(Game::IW4::PhysGeomInfo)); // This is ok: IW3 already has "bounds" the way iw4 has, for this struct precisely

				if (source->type >= 4)
				{
					// We're going from
					/*
						PHYS_GEOM_NONE = 0x0,
						PHYS_GEOM_BOX = 0x1,
						PHYS_GEOM_BRUSHMODEL = 0x2,
						PHYS_GEOM_BRUSH = 0x3,
						PHYS_GEOM_CYLINDER = 0x4,
						PHYS_GEOM_CAPSULE = 0x5,
							PHYS_GEOM_COUNT = 0x6,

					to

						PHYS_GEOM_NONE = 0x0,
						PHYS_GEOM_BOX = 0x1,
						PHYS_GEOM_BRUSHMODEL = 0x2,
						PHYS_GEOM_BRUSH = 0x3,
							PHYS_GEOM_COLLMAP = 0x4,
						PHYS_GEOM_CYLINDER = 0x5,
						PHYS_GEOM_CAPSULE = 0x6,
							PHYS_GEOM_GLASS = 0x7,
							PHYS_GEOM_COUNT = 0x8,
					*/

					target->type += 1;

					Components::Logger::Print("Translated physGeomType %i into %i (geom %i from model %s)\n", source->type, target->type, i, model->name);

					if (source->type >= 0x6)
					{
						Components::Logger::Error("Unexpected geom type %i (geom %i from model %s)\n", source->type, i, model->name);
					}
				}

				if (source->brush)
				{
					target->brush = LocalAllocator.Allocate<Game::IW4::BrushWrapper>();
					target->brush->bounds.compute(source->brush->mins, source->brush->maxs);

					target->brush->brush.numsides = static_cast<unsigned short>(source->brush->numsides);
					target->brush->brush.baseAdjacentSide = source->brush->baseAdjacentSide;
					target->brush->brush.glassPieceIndex = 0; // IW3's content is not mapped, but I doubt it belongs to this

					std::memcpy(target->brush->brush.axialMaterialNum, source->brush->axialMaterialNum, sizeof(source->brush->axialMaterialNum));
					std::memcpy(target->brush->brush.edgeCount, source->brush->edgeCount, sizeof(source->brush->edgeCount));

					for (int k = 0; k < 2; ++k)
					{
						for (int j = 0; j < 3; ++j)
						{
							target->brush->brush.firstAdjacentSideOffsets[k][j] = static_cast<char>(source->brush->firstAdjacentSideOffsets[k][j]);
						}
					}

					target->brush->planes = source->brush->planes;
					target->brush->totalEdgeCount = source->brush->totalEdgeCount;

					target->brush->brush.sides = LocalAllocator.AllocateArray<Game::IW4::cbrushside_t>(target->brush->brush.numsides);

					for (unsigned short j = 0; j < target->brush->brush.numsides; ++j)
					{
						Game::IW4::cbrushside_t* targetSide = &target->brush->brush.sides[j];
						Game::IW3::cbrushside_t* sourceSide = &source->brush->sides[j];

						targetSide->plane = sourceSide->plane;
						targetSide->materialNum = static_cast<unsigned short>(sourceSide->materialNum);
						targetSide->firstAdjacentSideOffset = static_cast<char>(sourceSide->firstAdjacentSideOffset);
						targetSide->edgeCount = sourceSide->edgeCount;
					}
				}
			}

			// Physical collision maps don't work!
			// Porting these on iw4 maps result in models with fucked up physics bouncing everywhere and making noise
			// and in some cases results in freezes if too many of them move over (fruits in carentan, papers on backlot)
			// Killing the physCollmap during porting solves this issue with no apparent drawback, surprisingly
			// The objects (car doors, fruits, ...) still work as expected when this is null. So let's keep it that way until we know what's going on!
			// The error may lie in the above codeblock, or in zonebuilder's reading of physcollmaps.
			xmodel.physCollmap = nullptr;
		}

		AddMissingViewHandsTag(&xmodel);
		AddMissingMultiplayerModelBones(&xmodel);
		AddShieldHitLocation(&xmodel);

		auto xmodelPtr = LocalAllocator.Allocate<Game::IW4::XModel>();
		*xmodelPtr = xmodel;

		return xmodelPtr;
	}

	void IXModel::AddMissingViewHandsTag(Game::IW4::XModel* model)
	{
		{
			const auto leftWrist = GetIndexOfBone(model, "j_wrist_le");
			const auto tagWeapon = GetIndexOfBone(model, "tag_weapon");
			if (leftWrist != UCHAR_MAX && tagWeapon != UCHAR_MAX)
			{
				if (GetIndexOfBone(model, "tag_weapon_left") == UCHAR_MAX)
				{
					// Missing tag_weapon_left, this will cause issues in IW4!
					const auto weaponLeft = InsertBone(model, "tag_weapon_left", "j_wrist_le", LocalAllocator);

					// "bad values" (this might not be important)
					SetBoneTrans(model, weaponLeft, false, 0.0f, 0.f, 0.f);
					SetBoneQuaternion(model, weaponLeft, false, 0.f, 0.0f, 0.f, 0.f);

					SetBoneTrans(model, weaponLeft, true, 0.0f, 0.f, 0.f);
					SetBoneQuaternion(model, weaponLeft, true, 0.f, 0.0f, 0.f, 0.f);

					RebuildPartBits(model);
				}
			}
		}
	}

	void IXModel::AddShieldHitLocation(Game::IW4::XModel* model)
	{
		constexpr unsigned char HITLOC_SHIELD = 0x13;

		{
			const auto tagWeaponLeft = GetIndexOfBone(model, "tag_weapon_left");
			if (tagWeaponLeft != UCHAR_MAX)
			{
				model->partClassification[tagWeaponLeft] = HITLOC_SHIELD;
			}
		}

		{
			const auto shieldBack = GetIndexOfBone(model, "tag_shield_back");
			if (shieldBack != UCHAR_MAX)
			{
				model->partClassification[shieldBack] = HITLOC_SHIELD;
			}
		}
	}

	void IXModel::AddMissingMultiplayerModelBones(Game::IW4::XModel* model)
	{
		const auto stowedBack = GetIndexOfBone(model, "tag_stowed_back");
		const auto hipTwistLeft = GetIndexOfBone(model, "j_hiptwist_le");
		if (stowedBack != UCHAR_MAX && hipTwistLeft != UCHAR_MAX)
		{
			if (GetIndexOfBone(model, "tag_shield_back") == UCHAR_MAX)
			{
				const auto parent = GetParentOfBone(model, stowedBack);
				const auto shieldBack = InsertBone(model, "tag_shield_back", parent, LocalAllocator);

				// Values taken from highrise

				SetBoneTrans(model, shieldBack, false, 0.0f, -8.43f, 0.801f);
				SetBoneQuaternion(model, shieldBack, false,
					-17151 / (float)SHRT_MAX,
					15578 / (float)SHRT_MAX,
					-15578 / (float)SHRT_MAX,
					17151 / (float)SHRT_MAX
				);

				SetBoneTrans(model, shieldBack, true, -11.713314f, 0.801846802f, 52.1543541f);
				SetBoneQuaternion(model, shieldBack, true,
					-0.0714808181f,
					-0.0f,
					-0.997450888f,
					-0.0f
				);
			}
		}

		RebuildPartBits(model);
	}


	uint8_t IXModel::GetIndexOfBone(const Game::IW4::XModel* model, std::string name)
	{
		for (uint8_t i = 0; i < model->numBones; i++)
		{
			const auto bone = model->boneNames[i];
			const auto boneName = Game::SL_ConvertToString(bone);
			if (name == boneName)
			{
				return i;
			}
		}

		return static_cast<uint8_t>(UCHAR_MAX);
	};

	uint8_t IXModel::GetParentIndexOfBone(const Game::IW4::XModel* model, uint8_t index)
	{
		const auto parentIndex = index - model->parentList[index - model->numRootBones];
		return static_cast<uint8_t>(parentIndex);
	};

	void IXModel::SetParentIndexOfBone(Game::IW4::XModel* model, uint8_t boneIndex, uint8_t parentIndex)
	{
		if (boneIndex == SCHAR_MAX)
		{
			return;
		}

		model->parentList[boneIndex - model->numRootBones] = boneIndex - parentIndex;
	};

	std::string IXModel::GetParentOfBone(Game::IW4::XModel* model, uint8_t index)
	{
		assert(index > 0);
		const auto parentIndex = GetParentIndexOfBone(model, index);
		const auto boneName = Game::SL_ConvertToString(model->boneNames[parentIndex]);
		return boneName;
	};

	uint8_t IXModel::GetHighestAffectingBoneIndex(const Game::IW4::XModelLodInfo* lod)
	{
		uint8_t highestBoneIndex = 0;

		{
			for (auto surfIndex = 0; surfIndex < lod->numsurfs; surfIndex++)
			{
				const auto surface = &lod->surfs[surfIndex];
				auto vertsBlendOffset = 0;

				std::unordered_set<uint8_t> affectingBones{};

				const auto registerBoneAffectingSurface = [&](unsigned int offset) {
					uint8_t index = static_cast<uint8_t>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::IW4::DObjSkelMat));
					highestBoneIndex = std::max(highestBoneIndex, index);
					};


				// 1 bone weight
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[0]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);

					vertsBlendOffset += 1;
				}

				// 2 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[1]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);

					vertsBlendOffset += 3;
				}

				// 3 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[2]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);
					registerBoneAffectingSurface(vertsBlendOffset + 3);

					vertsBlendOffset += 5;
				}

				// 4 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[3]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);
					registerBoneAffectingSurface(vertsBlendOffset + 3);
					registerBoneAffectingSurface(vertsBlendOffset + 5);

					vertsBlendOffset += 7;
				}

				for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; vertListIndex++)
				{
					highestBoneIndex = std::max(highestBoneIndex, static_cast<uint8_t>(surface->vertList[vertListIndex].boneOffset / sizeof(Game::IW4::DObjSkelMat)));
				}
			}
		}

		return highestBoneIndex;
	};

	void IXModel::RebuildPartBits(Game::IW4::XModel* model)
	{
		constexpr auto LENGTH = 6;

		for (auto i = 0; i < model->numLods; i++)
		{
			const auto lod = &model->lodInfo[i];
			int lodPartBits[6]{};

			for (unsigned short surfIndex = 0; surfIndex < lod->numsurfs; surfIndex++)
			{
				const auto surface = &lod->surfs[surfIndex];

				auto vertsBlendOffset = 0;

				int rebuiltPartBits[6]{};
				std::unordered_set<uint8_t> affectingBones{};

				const auto registerBoneAffectingSurface = [&](unsigned int offset) {
					uint8_t index = static_cast<uint8_t>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::IW4::DObjSkelMat));

					assert(index >= 0);
					assert(index < model->numBones);

					affectingBones.emplace(index);
					};


				// 1 bone weight
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[0]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);

					vertsBlendOffset += 1;
				}

				// 2 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[1]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);

					vertsBlendOffset += 3;
				}

				// 3 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[2]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);
					registerBoneAffectingSurface(vertsBlendOffset + 3);

					vertsBlendOffset += 5;
				}

				// 4 bone weights
				for (unsigned int vertIndex = 0; vertIndex < surface->vertInfo.vertCount[3]; vertIndex++)
				{
					registerBoneAffectingSurface(vertsBlendOffset + 0);
					registerBoneAffectingSurface(vertsBlendOffset + 1);
					registerBoneAffectingSurface(vertsBlendOffset + 3);
					registerBoneAffectingSurface(vertsBlendOffset + 5);

					vertsBlendOffset += 7;
				}

				for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; vertListIndex++)
				{
					affectingBones.emplace(static_cast<uint8_t>(surface->vertList[vertListIndex].boneOffset / sizeof(Game::IW4::DObjSkelMat)));
				}

				// Actually rebuilding
				for (const auto& boneIndex : affectingBones)
				{
					const auto bitPosition = 31 - boneIndex % 32;
					const auto groupIndex = boneIndex / 32;

					assert(groupIndex < 6);
					assert(groupIndex >= 0);

					rebuiltPartBits[groupIndex] |= 1 << bitPosition;
					lodPartBits[groupIndex] |= 1 << bitPosition;
				}

				std::memcpy(surface->partBits, rebuiltPartBits, 6 * sizeof(int32_t));
			}

			std::memcpy(lod->partBits, lodPartBits, 6 * sizeof(int32_t));
			std::memcpy(lod->modelSurfs->partBits, lodPartBits, 6 * sizeof(int32_t));

			// here's a little lesson in trickery:
			// We set the 192nd part bit to TRUE because it has no consequences
			//	but allows us to find out whether that surf was already converted in the past or not
			lod->partBits[LENGTH - 1] |= 0x1;
			lod->modelSurfs->partBits[LENGTH - 1] |= 0x1;
		}
	};


	uint8_t IXModel::InsertBone(Game::IW4::XModel* model, const std::string& boneName, const std::string& parentName, Utils::Memory::Allocator& allocator)
	{
		assert(GetIndexOfBone(model, boneName) == UCHAR_MAX);

#if DEBUG
		constexpr auto MAX_BONES = 192;
		assert(model->numBones < MAX_BONES);
#endif

		// Start with backing up parent links that we will have to restore
		// We'll restore them at the end
		std::map<std::string, std::string> parentsToRestore{};
		for (uint8_t i = model->numRootBones; i < model->numBones; i++)
		{
			parentsToRestore[Game::SL_ConvertToString(model->boneNames[i])] = GetParentOfBone(model, i);
		}

		const uint8_t newBoneCount = model->numBones + 1;
		const uint8_t newBoneCountMinusRoot = newBoneCount - model->numRootBones;

		const auto parentIndex = GetIndexOfBone(model, parentName);

		assert(parentIndex != UCHAR_MAX);

		const uint8_t atPosition = parentIndex + 1;

		const uint8_t newBoneIndex = atPosition;
		const uint8_t newBoneIndexMinusRoot = atPosition - model->numRootBones;

		// Reallocate
		const auto newBoneNames = allocator.AllocateArray<uint16_t>(newBoneCount);
		const auto newMats = allocator.AllocateArray<Game::IW3::DObjAnimMat>(newBoneCount);
		const auto newBoneInfo = allocator.AllocateArray<Game::IW4::XBoneInfo>(newBoneCount);
		const auto newPartsClassification = allocator.AllocateArray<uint8_t>(newBoneCount);
		const auto newQuats = allocator.AllocateArray<int16_t>(4 * newBoneCountMinusRoot);
		const auto newTrans = allocator.AllocateArray<float>(3 * newBoneCountMinusRoot);
		const auto newParentList = allocator.AllocateArray<uint8_t>(newBoneCountMinusRoot);

		const uint8_t lengthOfFirstPart = atPosition;
		const uint8_t lengthOfSecondPart = model->numBones - atPosition;

		const uint8_t lengthOfFirstPartM1 = atPosition - model->numRootBones;
		const uint8_t lengthOfSecondPartM1 = model->numBones - model->numRootBones - (atPosition - model->numRootBones);

		const uint8_t atPositionM1 = atPosition - model->numRootBones;

#if DEBUG
		// should be equal to model->numBones
		unsigned int total = lengthOfFirstPart + lengthOfSecondPart;
		assert(total == model->numBones);

		// should be equal to model->numBones - model->numRootBones
		int totalM1 = lengthOfFirstPartM1 + lengthOfSecondPartM1;
		assert(totalM1 == model->numBones - model->numRootBones);
#endif

		// Copy before
		if (lengthOfFirstPart > 0)
		{
			std::memcpy(newBoneNames, model->boneNames, sizeof(uint16_t) * lengthOfFirstPart);
			std::memcpy(newMats, model->baseMat, sizeof(Game::IW3::DObjAnimMat) * lengthOfFirstPart);
			std::memcpy(newPartsClassification, model->partClassification, lengthOfFirstPart);
			std::memcpy(newBoneInfo, model->boneInfo, sizeof(Game::IW4::XBoneInfo) * lengthOfFirstPart);
			std::memcpy(newQuats, model->quats, sizeof(uint16_t) * 4 * lengthOfFirstPartM1);
			std::memcpy(newTrans, model->trans, sizeof(float) * 3 * lengthOfFirstPartM1);
		}

		// Insert new bone
		{
			unsigned int name = Game::SL_GetStringOfSize(boneName.data(), 0, boneName.size() + 1);
			Game::IW4::XBoneInfo boneInfo{};

			Game::IW3::DObjAnimMat mat{};

			// It's ABSOLUTE!
			mat = model->baseMat[parentIndex];

			boneInfo = model->boneInfo[parentIndex];

			// It's RELATIVE !
			uint16_t quat[4]{};
			quat[3] = SHRT_MAX; // 0 0 0 1

			float trans[3]{};

			mat.transWeight = 1.9999f; // Should be 1.9999 like everybody?

			newMats[newBoneIndex] = mat;
			newBoneInfo[newBoneIndex] = boneInfo;
			newBoneNames[newBoneIndex] = static_cast<uint16_t>(name);

			// TODO parts Classification

			std::memcpy(&newQuats[newBoneIndexMinusRoot * 4], quat, ARRAYSIZE(quat) * sizeof(uint16_t));
			std::memcpy(&newTrans[newBoneIndexMinusRoot * 3], trans, ARRAYSIZE(trans) * sizeof(float));
		}

		// Copy after
		if (lengthOfSecondPart > 0)
		{
			std::memcpy(&newBoneNames[atPosition + 1], &model->boneNames[atPosition], sizeof(uint16_t) * lengthOfSecondPart);
			std::memcpy(&newMats[atPosition + 1], &model->baseMat[atPosition], sizeof(Game::IW3::DObjAnimMat) * lengthOfSecondPart);
			std::memcpy(&newPartsClassification[atPosition + 1], &model->partClassification[atPosition], lengthOfSecondPart);
			std::memcpy(&newBoneInfo[atPosition + 1], &model->boneInfo[atPosition], sizeof(Game::IW4::XBoneInfo) * lengthOfSecondPart);
			std::memcpy(&newQuats[(atPositionM1 + 1) * 4], &model->quats[atPositionM1 * 4], sizeof(uint16_t) * 4 * lengthOfSecondPartM1);
			std::memcpy(&newTrans[(atPositionM1 + 1) * 3], &model->trans[atPositionM1 * 3], sizeof(float) * 3 * lengthOfSecondPartM1);
		}

		//Game::Z_VirtualFree(model->baseMat);
		//Game::Z_VirtualFree(model->boneInfo);
		//Game::Z_VirtualFree(model->boneNames);
		//Game::Z_VirtualFree(model->quats);
		//Game::Z_VirtualFree(model->trans);
		//Game::Z_VirtualFree(model->parentList);

		// Assign reallocated
		model->baseMat = newMats;
		model->boneInfo = newBoneInfo;
		model->boneNames = newBoneNames;
		model->quats = newQuats;
		model->trans = newTrans;
		model->parentList = newParentList;

		model->numBones = newBoneCount;

		// Update vertex weight
		for (uint8_t lodIndex = 0; lodIndex < model->numLods; lodIndex++)
		{
			const auto lod = &model->lodInfo[lodIndex];

			if ((lod->modelSurfs->partBits[5] & 0x1) == 0x1)
			{
				// surface lod already converted (more efficient)
				std::memcpy(lod->partBits, lod->modelSurfs->partBits, 6 * sizeof(uint32_t));
				continue;
			}

			if (GetHighestAffectingBoneIndex(lod) >= model->numBones)
			{
				// surface lod already converted (more accurate)
				continue;
			}

			for (int surfIndex = 0; surfIndex < lod->modelSurfs->numSurfaces; surfIndex++)
			{
				auto vertsBlendOffset = 0u;

				const auto surface = &lod->modelSurfs->surfaces[surfIndex];

				static_assert(sizeof(Game::IW4::DObjSkelMat) == 64);

				{
					const auto fixVertexBlendIndex = [&](unsigned int offset) {

						int index = static_cast<int>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::IW4::DObjSkelMat));

						if (index >= atPosition)
						{
							index++;

							if (index < 0 || index >= model->numBones)
							{
								Components::Logger::Print("Unexpected 'bone index' {} out of {} bones while working vertex blend of: xmodel {} lod {} xmodelsurf {} surf #{}", index, model->numBones, model->name, lodIndex, lod->modelSurfs->name, lodIndex, surfIndex);
								assert(false);
							}

							surface->vertInfo.vertsBlend[offset] = static_cast<unsigned short>(index * sizeof(Game::IW4::DObjSkelMat));
						}
						};

					//  Fix bone offsets
					if (surface->vertList)
					{
						for (auto vertListIndex = 0u; vertListIndex < surface->vertListCount; vertListIndex++)
						{
							const auto vertList = &surface->vertList[vertListIndex];

							auto index = vertList->boneOffset / sizeof(Game::IW4::DObjSkelMat);
							if (index >= atPosition)
							{
								index++;

								if (index < 0 || index >= model->numBones)
								{
									Components::Logger::Print("Unexpected 'bone index' {} out of {} bones while working vertex list of: xmodel {} lod {} xmodelsurf {} surf #{}\n", index, model->numBones, model->name, lodIndex, lod->modelSurfs->name, surfIndex);
									assert(false);
								}

								vertList->boneOffset = static_cast<unsigned short>(index * sizeof(Game::IW4::DObjSkelMat));
							}
						}
					}

					// 1 bone weight
					for (auto vertIndex = 0; vertIndex < surface->vertInfo.vertCount[0]; vertIndex++)
					{
						fixVertexBlendIndex(vertsBlendOffset + 0);

						vertsBlendOffset += 1;
					}

					// 2 bone weights
					for (auto vertIndex = 0; vertIndex < surface->vertInfo.vertCount[1]; vertIndex++)
					{
						fixVertexBlendIndex(vertsBlendOffset + 0);
						fixVertexBlendIndex(vertsBlendOffset + 1);

						vertsBlendOffset += 3;
					}

					// 3 bone weights
					for (auto vertIndex = 0; vertIndex < surface->vertInfo.vertCount[2]; vertIndex++)
					{
						fixVertexBlendIndex(vertsBlendOffset + 0);
						fixVertexBlendIndex(vertsBlendOffset + 1);
						fixVertexBlendIndex(vertsBlendOffset + 3);

						vertsBlendOffset += 5;
					}

					// 4 bone weights
					for (auto vertIndex = 0; vertIndex < surface->vertInfo.vertCount[3]; vertIndex++)
					{
						fixVertexBlendIndex(vertsBlendOffset + 0);
						fixVertexBlendIndex(vertsBlendOffset + 1);
						fixVertexBlendIndex(vertsBlendOffset + 3);
						fixVertexBlendIndex(vertsBlendOffset + 5);

						vertsBlendOffset += 7;
					}
				}
			}
		}

		SetParentIndexOfBone(model, atPosition, parentIndex);

		// Restore parents
		for (const auto& kv : parentsToRestore)
		{
			// Fix parents
			const auto key = kv.first;
			const auto beforeVal = kv.second;

			const auto p = GetIndexOfBone(model, beforeVal);
			const auto index = GetIndexOfBone(model, key);
			SetParentIndexOfBone(model, index, p);
		}

		return atPosition; // Bone index of added bone
	};


	void IXModel::SetBoneTrans(Game::IW4::XModel* model, uint8_t boneIndex, bool baseMat, float x, float y, float z)
	{
		if (baseMat)
		{
			model->baseMat[boneIndex].trans[0] = x;
			model->baseMat[boneIndex].trans[1] = y;
			model->baseMat[boneIndex].trans[2] = z;
		}
		else
		{
			const auto index = boneIndex - model->numRootBones;
			assert(index >= 0);

			model->trans[index * 3 + 0] = x;
			model->trans[index * 3 + 1] = y;
			model->trans[index * 3 + 2] = z;
		}
	}

	void IXModel::SetBoneQuaternion(Game::IW4::XModel* model, uint8_t boneIndex, bool baseMat, float x, float y, float z, float w)
	{
		if (baseMat)
		{
			model->baseMat[boneIndex].quat[0] = x;
			model->baseMat[boneIndex].quat[1] = y;
			model->baseMat[boneIndex].quat[2] = z;
			model->baseMat[boneIndex].quat[3] = w;
		}
		else
		{
			const auto index = boneIndex - model->numRootBones;
			assert(index >= 0);

			model->quats[index * 4 + 0] = static_cast<uint16_t>(x * SHRT_MAX);
			model->quats[index * 4 + 1] = static_cast<uint16_t>(y * SHRT_MAX);
			model->quats[index * 4 + 2] = static_cast<uint16_t>(z * SHRT_MAX);
			model->quats[index * 4 + 3] = static_cast<uint16_t>(w * SHRT_MAX);
		}
	}


	IXModel::IXModel()
	{

		Command::Add("dumpXModel", [](const Command::Params& params)
			{
				if (params.Length() < 2) return;

				Game::DB_SyncXAssets();

				if ("*"s == params[1])
				{
					std::vector<std::string> names{};

					Game::DB_EnumXAssetEntries(Game::IW3::XAssetType::ASSET_TYPE_XMODEL, [&](Game::IW3::XAssetEntryPoolEntry* poolEntry) {
						if (poolEntry)
						{
							auto entry = &poolEntry->entry;
							if (entry->inuse == 1 && entry->asset.header.data && entry->asset.header.model->name)
							{
								names.emplace_back(entry->asset.header.model->name);
							}
						}
						}, false);

					for (const auto& name : names)
					{
						const auto entry = Game::DB_FindXAssetEntry(Game::IW3::XAssetType::ASSET_TYPE_XMODEL, name.data())->entry.asset.header;

						auto converted = IXModel::Convert(entry.model);
						MapDumper::GetApi()->write(Game::IW4::ASSET_TYPE_XMODEL, converted);
					}

					return;
				}

				auto iw3Model = Game::DB_FindXAssetHeader(Game::IW3::XAssetType::ASSET_TYPE_XMODEL, params[1]).model;
				auto converted = IXModel::Convert(iw3Model);
				MapDumper::GetApi()->write(Game::IW4::ASSET_TYPE_XMODEL, converted);
			});
	}

	IXModel::~IXModel()
	{

	}

	void IXModel::Reset()
	{
		convertedModelSurfaces.clear();
	}
}
