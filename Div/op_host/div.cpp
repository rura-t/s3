
#include "div_tiling.h"
#include "register/op_def_registry.h"
#include "tiling/platform/platform_ascendc.h"

const uint32_t BLOCK_SIZE = 32;

namespace optiling {
static ge::graphStatus TilingFunc(gert::TilingContext* context)
{

    DivTilingData tiling;
    int32_t NUM = 3;
    auto ascendcPlatform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
    uint64_t ub_size; ascendcPlatform.GetCoreMemSize(platform_ascendc::CoreMemType::UB, ub_size);
    auto aivNum = ascendcPlatform.GetCoreNum();
    std::cout << "ub_size" << ub_size << "   aivNum: " << aivNum << std::endl;
    uint32_t total_length = 0, min_length = context->GetInputTensor(0)->GetShapeSize();
    for (int i = 0; i < 2; ++i) {
        total_length = std::max<uint32_t>(total_length, context->GetInputTensor(i)->GetShapeSize());
        min_length = std::min<uint32_t>(min_length, context->GetInputTensor(i)->GetShapeSize());
    }
    uint32_t x1_length = context->GetInputTensor(0)->GetShapeSize();
    uint32_t x2_length = context->GetInputTensor(1)->GetShapeSize();
    auto dt = context->GetInputTensor(0)->GetDataType();
    uint32_t sizeofdatatype;
    if (dt == ge::DT_INT8) {
        sizeofdatatype = 1;
        NUM = 7;
    }
    else if (dt == ge::DT_FLOAT16 || dt == ge::DT_BF16) {
        sizeofdatatype = 2;
    }
    else {
        sizeofdatatype = 4;
    }

    uint32_t ALIGN_NUM = BLOCK_SIZE / sizeofdatatype;
    uint32_t tiling_size = ((ub_size) / BLOCK_SIZE / 2) / NUM;
    tiling_size = tiling_size <= 8 ? tiling_size : tiling_size / 8 * 8;

    uint32_t block_size = tiling_size * ALIGN_NUM;
    if (total_length != min_length) {
        block_size = std::min(block_size, min_length);
        while (min_length % block_size || min_length % ALIGN_NUM) {
            block_size -= 1;
        }
    }

    aivNum = (aivNum < total_length / block_size) ? aivNum : (total_length / block_size);
    aivNum = aivNum >= 1 ? aivNum : 1;

    uint32_t core_size = (total_length / aivNum) / (ALIGN_NUM * 8) * (ALIGN_NUM * 8);
    uint32_t core_remain = total_length - aivNum * core_size;

    tiling.set_ALIGN_NUM(ALIGN_NUM);
    tiling.set_block_size(block_size);
    tiling.set_aivNum(aivNum);
    tiling.set_core_size(core_size);
    tiling.set_core_remain(core_remain);
    tiling.set_total_length(total_length);
    tiling.set_x1_length(x1_length);
    tiling.set_x2_length(x2_length);

    context->SetBlockDim(aivNum);

    printf("##########################################################################\n");
    printf("                            THIS IS TILING DATAS\n"); 
    std::cout << "++ " << "total_length = " << total_length << "\n";
    std::cout << "++ " << "x1_length = " << x1_length << "\n";
    std::cout << "++ " << "x2_length = " << x2_length << "\n";
    std::cout << "++ " << "ALIGN_NUM = " << ALIGN_NUM << "\n";  
    std::cout << "++ " << "ub_size = " << ub_size << "\n";  
    std::cout << "++ " << "tiling_size = " << tiling_size << "\n";  
    std::cout << "++ " << "block_size = " << block_size << "\n"; 
    std::cout << "++ " << "aivNum = " << aivNum << "\n"; 
    std::cout << "++ " << "core_size = " << core_size << "\n"; 
    std::cout << "++ " << "core_remain = " << core_remain << "\n"; 
    printf("##########################################################################\n");

    tiling.SaveToBuffer(context->GetRawTilingData()->GetData(), context->GetRawTilingData()->GetCapacity());
    context->GetRawTilingData()->SetDataSize(tiling.GetDataSize());
    size_t *currentWorkspace = context->GetWorkspaceSizes(1);
    currentWorkspace[0] = 0;
    return ge::GRAPH_SUCCESS;
}
}


namespace ge {
static ge::graphStatus InferShape(gert::InferShapeContext* context)
{
    const gert::Shape* x1_shape = context->GetInputShape(0);
    gert::Shape* y_shape = context->GetOutputShape(0);
    *y_shape = *x1_shape;
    return GRAPH_SUCCESS;
}
}


namespace ops {
class Div : public OpDef {
public:
    explicit Div(const char* name) : OpDef(name)
    {
        this->Input("x1")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT32, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("x2")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT32, ge::DT_INT8})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Output("y")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT32, ge::DT_INT8})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});

        this->SetInferShape(ge::InferShape);

        this->AICore()
            .SetTiling(optiling::TilingFunc);
        this->AICore().AddConfig("ascend310b");

    }
};

OP_ADD(Div);
}
