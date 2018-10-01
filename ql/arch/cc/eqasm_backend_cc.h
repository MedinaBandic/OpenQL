/**
 * @file   eqasm_backend_cc
 * @date   201809xx
 * @author Wouter Vlothuizen (wouter.vlothuizen@tno.nl)
 * @brief  eqasm backend for the Central Controller
 * @remark based on cc_light_eqasm_compiler.h
 */

#ifndef QL_ARCH_CC_EQASM_BACKEND_CC_H
#define QL_ARCH_CC_EQASM_BACKEND_CC_H

#include <ql/platform.h>
#include <ql/ir.h>
#include <ql/circuit.h>
#include <ql/scheduler.h>
#include <ql/eqasm_compiler.h>
#include <ql/arch/cc_light/cc_light_resource_manager.h>



namespace ql
{
namespace arch
{

class eqasm_backend_cc : public eqasm_compiler
{
private:
    // parameters from JSON file:
    size_t qubit_number;    // num_qubits;
    size_t cycle_time;      // ns_per_cycle;
    size_t buffer_matrix[__operation_types_num__][__operation_types_num__];

    size_t total_exec_time = 0;
    bool verbose = true;    // output extra comments in generated code

public:
    /*
     * compile for Central Controller (CCCODE)
     * NB: based on cc_light_eqasm_compiler.h, commit f34c0d9
     */

    void compile(std::string prog_name, std::vector<quantum_kernel> kernels, ql::quantum_platform& platform)
    {
#if 1   // FIXME: patch for issue #164, should be in caller
        if(kernels.size() == 0) {
            FATAL("Trying to compile empty kernel");
        }
#endif
        DOUT("Compiling " << kernels.size() << " kernels to generate CCCODE ... ");
        load_backend_settings(platform);
        load_hw_settings(platform);

        // generate program header
        std::stringstream cccode;
        cccode << "# Program: '" << prog_name << "'" << std::endl;
        cccode << "# Note:    generated by OpenQL Central Controller backend" << std::endl;
        cccode << "#" << std::endl;

        // generate code for all kernels
        for(auto &kernel : kernels) {
            IOUT("Compiling kernel: " << kernel.name);
            if(verbose) cccode << "# Kernel:  " << kernel.name << std::endl;
            cccode << get_prologue(kernel);

            ql::circuit& ckt = kernel.c;
            if (!ckt.empty()) {
                auto creg_count = kernel.creg_count;                        // FIXME: also take platform into account
                ql::circuit decomp_ckt;

                decompose_instructions(ckt, decomp_ckt, platform);          // decompose meta-instructions

#if 0   // FIXME: based on old code, disabled in cc_light_scheduler.h
                // schedule
                ql::ir::bundles_t bundles = cc_light_schedule(decomp_ckt, platform, qubit_number, creg_count);
#else
                // schedule with platform resource constraints
                ql::ir::bundles_t bundles = cc_light_schedule_rc(decomp_ckt, platform, qubit_number, creg_count);
                // FIXME: cc_light* is just available here because everything is in header files
#endif
                cccode << bundles2cccode(bundles, platform);
            } else {
                DOUT("Empty kernel: " << kernel.name);                      // NB: normal situation for kernels with classical control
            }

            cccode << get_epilogue(kernel);
        }

        emit(cccode, "", "stop");                                  // FIXME: cc_light loops whole program indefinitely

        // write CCCODE to file
        std::string file_name(ql::options::get("output_dir") + "/" + prog_name + ".cccode");
        IOUT("Writing CCCODE to " << file_name);
        ql::utils::write_file(file_name, cccode.str());

        DOUT("Compiling CCCODE [Done]");
    }


    void compile(std::string prog_name, ql::circuit& ckt, ql::quantum_platform& platform)
    {
        FATAL("circuit compilation not implemented, because it does not support classical kernel operations");
    }


#if 0   // FIXME: potential additions, from cc_light
    // time analysis
    // total_exec_time = time_analysis();

    // compensate for latencies
    // compensate_latency();

    // reschedule
    // resechedule();

    // dump_instructions();

    // decompose meta-instructions
    // decompose_instructions();

    // reorder instructions
    // reorder_instructions();

    // insert waits

    emit_eqasm();
#endif




private:
    // some helpers to ease nice assembly formatting
    void emit(std::stringstream &ss, const char *labelOrComment, const char *instr="")
    {
        ss << std::left;    // FIXME
        if(!labelOrComment || strlen(labelOrComment)==0) {  // no label
            ss << "        " << instr << std::endl;
        } else if(strlen(labelOrComment)<8) {               // label fits before instr
            ss << std::setw(8) << labelOrComment << instr << std::endl;
        } else if(strlen(instr)==0) {                       // no instr
            ss << labelOrComment << std::endl;
        } else {
            ss << labelOrComment << std::endl << "        " << instr << std::endl;
        }
    }

    // @param   label       must include trailing ":"
    // @param   comment     must include leading "#"
    void emit(std::stringstream &ss, const char *label, const char *instr, std::string ops, const char *comment="")
    {
        ss << std::left;    // FIXME
        ss << std::setw(8) << label << std::setw(8) << instr << std::setw(16) << ops << comment << std::endl;
    }


    void load_backend_settings(ql::quantum_platform& platform)
    {
        // FIXME: we would like to have a top level setting, or one below "backends"
        // it is however not easy to create new top level stuff and read it from the backend
        try
        {
            auto backendSettings = platform.hardware_settings["eqasm_backend_cc"];
            auto test = backendSettings["test"];
            DOUT("load_backend_settings read key 'test:'" << test);
        }
        catch (json::exception e)
        {
#if 0
            throw ql::exception(
                "[x] error : ql::eqasm_compiler::compile() : error while reading backend settings : parameter '"
//                + hw_settings[i].name
                + "'\n\t"
                + std::string(e.what()), false);
#endif
        }
    }


    // based on: cc_light_eqasm_compiler.h::load_hw_settings
    void load_hw_settings(ql::quantum_platform& platform)
    {
        const struct {
            size_t  *var;
            std::string name;
        } hw_settings[] = {
            { &qubit_number,            "qubit_number"},
            { &cycle_time,              "cycle_time" },
#if 0   // FIXME: unused. Convert to cycle
            { &mw_mw_buffer,            "mw_mw_buffer" },
            { &mw_flux_buffer,          "mw_flux_buffer" },
            { &mw_readout_buffer,       "mw_readout_buffer" },
            { &flux_mw_buffer,          "flux_mw_buffer" },
            { &flux_flux_buffer,        "flux_flux_buffer" },
            { &flux_readout_buffer,     "flux_readout_buffer" },
            { &readout_mw_buffer,       "readout_mw_buffer" },
            { &readout_flux_buffer,     "readout_flux_buffer" },
            { &readout_readout_buffer,  "readout_readout_buffer" }
#endif
        };

        DOUT("Loading hardware settings ...");
        int i=0;
        try
        {
            for(i=0; i<sizeof(hw_settings)/sizeof(hw_settings[0]); i++) {
                size_t val = platform.hardware_settings[hw_settings[i].name];
                *hw_settings[i].var = val;
            }
        }
        catch (json::exception e)
        {
            throw ql::exception(
                "[x] error : ql::eqasm_compiler::compile() : error while reading hardware settings : parameter '"
                + hw_settings[i].name
                + "'\n\t"
                + std::string(e.what()), false);
        }
    }


    /* decompose meta-instructions
        Parameters:
        ckt     input circuit, containing ... FIXME
     */
    // FIXME: what/which are the meta-instructions and where are they defined. Why aren't they decomposed on code generation
    // based on: cc_light_eqasm_compiler.h::decompose_instructions
    // FIXME: return decomp_ckt
    // FIXME: maybe split off code generation, the rest can be generic to several backends
    void decompose_instructions(ql::circuit &ckt, ql::circuit &decomp_ckt, ql::quantum_platform &platform)
    {
        DOUT("decomposing instructions...");
        for( auto ins : ckt )
        {
            auto & iname =  ins->name;
            str::lower_case(iname);
            DOUT("decomposing instruction " << iname << "...");
            auto & iopers = ins->operands;
            int iopers_count = iopers.size();
            auto itype = ins->type();
            if(__classical_gate__ == itype)
            {
#if 0   // now handled by classical_instruction2cccode
                DOUT("    classical instruction");

                if( (iname == "add") || (iname == "sub") ||
                    (iname == "and") || (iname == "or") || (iname == "xor") ||
                    (iname == "not") || (iname == "nop")
                  )
                {
                    // decomp_ckt.push_back(ins);
     // FIXME: classical_cc is part of cc_light
                   decomp_ckt.push_back(new ql::arch::classical_cc(iname, iopers));
                }
                else if( (iname == "eq") || (iname == "ne") || (iname == "lt") ||
                         (iname == "gt") || (iname == "le") || (iname == "ge")
                       )
                {
                    decomp_ckt.push_back(new ql::arch::classical_cc("cmp", {iopers[1], iopers[2]}));
                    decomp_ckt.push_back(new ql::arch::classical_cc("nop", {}));
                    decomp_ckt.push_back(new ql::arch::classical_cc("fbr_"+iname, {iopers[0]}));
                }
                else if(iname == "mov")
                {
                    // r28 is used as temp, TODO use creg properly to create temporary
                    decomp_ckt.push_back(new ql::arch::classical_cc("ldi", {28}, 0));
                    decomp_ckt.push_back(new ql::arch::classical_cc("add", {iopers[0], iopers[1], 28}));
                }
                else if(iname == "ldi")
                {
                    auto imval = ((ql::classical*)ins)->imm_value;
                    decomp_ckt.push_back(new ql::arch::classical_cc("ldi", iopers, imval));
                }
                else
                {
                    EOUT("Unknown decomposition of classical operation '" << iname << "' with '" << iopers_count << "' operands!");
                    throw ql::exception("Unknown classical operation '"+iname+"' with'"+std::to_string(iopers_count)+"' operands!", false);
                }
#endif
            }
            else
            {
                if(iname == "wait")
                {
                    DOUT("    wait instruction ");
                    decomp_ckt.push_back(ins);
                }
                else
                {
                    json& instruction_settings = platform.instruction_settings;
                    std::string operation_type;
                    if (instruction_settings.find(iname) != instruction_settings.end())
                    {
                        operation_type = instruction_settings[iname]["type"];
                    }
                    else
                    {
                        FATAL("instruction settings not found for '" << iname << "' with'" << iopers_count << "' operands!");
                    }
                    bool is_measure = (operation_type == "readout");
                    if(is_measure)
                    {
                        // insert measure
                        DOUT("    readout instruction ");
                        auto qop = iopers[0];
                        decomp_ckt.push_back(ins);
                        if( ql::gate_type_t::__custom_gate__ == itype )
                        {
                            auto & coperands = ins->creg_operands;
                            if(!coperands.empty())
                            {
                                auto cop = coperands[0];
                                decomp_ckt.push_back(new ql::arch::classical_cc("fmr", {cop, qop}));
                            }
                            else
                            {
                                WOUT("Unknown classical operand for measure/readout operation: '" << iname <<
                                    ". This will soon be depricated in favour of measure instruction with fmr" <<
                                    " to store measurement outcome to classical register.");
                            }
                        }
                        else
                        {
                            FATAL("Unknown decomposition of measure/readout operation '" << iname << "'!");
                        }
                    }
                    else
                    {
                        DOUT("    quantum instruction ");
                        decomp_ckt.push_back(ins);
                    }
                }
            }
        }

#if 0   // originally commented out
        cc_light_eqasm_program_t decomposed;
        for (cc_light_eqasm_instruction * instr : cc_light_eqasm_instructions)
        {
        cc_light_eqasm_program_t dec = instr->decompose();
          for (cc_light_eqasm_instruction * i : dec)
             decomposed.push_back(i);
            }
            cc_light_eqasm_instructions.swap(decomposed);
#endif
        DOUT("decomposing instructions...[Done]");
    }


    // based on cc_light_eqasm_compiler.h::classical_instruction2qisa
    // NB: input instructions defined in classical.h::classical (also in JSON???)
    // also see cc_light_eqasm_compiler.h::decompose_instructions(), which produces "fmr" and friends
    std::string classical_instruction2cccode(ql::gate *classical_ins)
    {
        std::stringstream ssclassical;
        auto & iname =  classical_ins->name;
        auto & iopers = classical_ins->operands;
        int iopers_count = iopers.size();

        if(  (iname == "add") || (iname == "sub") ||
             (iname == "and") || (iname == "or") || (iname == "not") || (iname == "xor") ||
             (iname == "ldi") || (iname == "nop") || (iname == "cmp")
          )
        {
            ssclassical << iname;
            for(int i=0; i<iopers_count; ++i)
            {
                if(i==iopers_count-1)
                    ssclassical << " r" <<  iopers[i];
                else
                    ssclassical << " r" << iopers[i] << ",";
            }
            if(iname == "ldi")
            {
//                ssclassical << ", " + std::to_string(classical_ins->imm_value);
            }
        }
#if 0   // inserted from decompose_instructions
        else if( (iname == "eq") || (iname == "ne") || (iname == "lt") ||
                 (iname == "gt") || (iname == "le") || (iname == "ge")
               )
        else if(iname == "mov")
        {
            // r28 is used as temp, TODO use creg properly to create temporary
            decomp_ckt.push_back(new ql::arch::classical_cc("ldi", {28}, 0));
            decomp_ckt.push_back(new ql::arch::classical_cc("add", {iopers[0], iopers[1], 28}));
        }
        else if(iname == "ldi")
#endif
        else
        {
            FATAL("Unknown classical operation'" << iname << "' with'" << iopers_count << "' operands!");
        }

        return ssclassical.str();
    }


    // get label from kernel name: FIXME: the label is the program name
    // FIXME: the kernel name has a structure (e.g. "sp1_for1_start" or "sp1_for1_start") which we use here. This should be made explicit
    // FIXME: looks very inefficient
    // extracted from get_epilogue
    std::string kernelLabel(ql::quantum_kernel &k)
    {
        std::string kname(k.name);
        std::replace(kname.begin(), kname.end(), '_', ' ');
        std::istringstream iss(kname);
        std::vector<std::string> tokens{ std::istream_iterator<std::string>{iss},
                                         std::istream_iterator<std::string>{} };
        return tokens[0];
    }


    // based on cc_light_eqasm_compiler.h::get_prologue
    std::string get_prologue(ql::quantum_kernel &k)
    {
        std::stringstream ss;

        switch(k.type) {
            case kernel_type_t::IF_START:
            {
                auto op0 = k.br_condition.operands[0]->id;
                auto op1 = k.br_condition.operands[1]->id;
                auto opName = k.br_condition.operation_name;
                if(verbose) ss << "# IF_START(R" << op0 << " " << opName << " R" << op1 << ")" << std::endl;
                // FIXME: implement
                break;
            }

            case kernel_type_t::ELSE_START:
            {
                auto op0 = k.br_condition.operands[0]->id;
                auto op1 = k.br_condition.operands[1]->id;
                auto opName = k.br_condition.operation_name;
                if(verbose) ss << "# ELSE_START(R" << op0 << " " << opName << " R" << op1 << ")" << std::endl;
                // FIXME: implement
                break;
            }

            case kernel_type_t::FOR_START:
            {
                std::string label = kernelLabel(k);
                if(verbose) ss << "# FOR_START(" << k.iterations << ")" << std::endl;
                emit(ss, (label+":").c_str(), "move", SS2S(k.iterations << ",R63"), "# R63 is the 'for loop counter'");        // FIXME: fixed reg, no nested loops
                break;
            }

            case kernel_type_t::DO_WHILE_START:
            {
                std::string label = kernelLabel(k);
                if(verbose) ss << "# DO_WHILE_START" << std::endl;
                // FIXME: implement: emit label
                break;
            }

            default:
                // nothing to do for other types
                break;
        }
        return ss.str();
    }


    // based on cc_light_eqasm_compiler.h::get_epilogue
    std::string get_epilogue(ql::quantum_kernel &k)
    {
        std::stringstream ss;

        switch(k.type) {
            case kernel_type_t::FOR_END:
            {
                std::string label = kernelLabel(k);
                if(verbose) emit(ss, "# FOR_END");
                emit(ss, "", "loop", SS2S("R63,@" << label), "# R63 is the 'for loop counter'");        // FIXME: fixed reg, no nested loops
                break;
            }

            case kernel_type_t::DO_WHILE_END:
            {
                auto op0 = k.br_condition.operands[0]->id;
                auto op1 = k.br_condition.operands[1]->id;
                auto opName = k.br_condition.operation_name;
                if(verbose) ss << "# DO_WHILE_END(R" << op0 << " " << opName << " R" << op1 << ")" << std::endl;
                // FIXME: implement
                break;
            }

            default:
                // nothing to do for other types
                break;
        }
        return ss.str();
    }


    // based on cc_light_eqasm_compiler.h::bundles2qisa()
    std::string bundles2cccode(ql::ir::bundles_t &bundles, ql::quantum_platform &platform)
    {
        IOUT("Generating CCCODE for bundles");

        std::stringstream ssbundles;
        size_t curr_cycle = 0;

        for(ql::ir::bundle_t &bundle : bundles)
        {
            auto delta = bundle.start_cycle - curr_cycle;
            bool classical_bundle = false;
            std::stringstream sspre, ssinst;

            // generate bindle header
#if 0   // FIXME: later. Is it necessary to split sspre and ssinst?
            // delay start of bundle
            if(delta < 8)
                sspre << "    " << delta << "    ";
            else
                sspre << "    qwait " << delta-1 << "\n"
                      << "    1    ";
#endif

            // generate code for this bundle
            for(auto section = bundle.parallel_sections.begin(); section != bundle.parallel_sections.end(); ++section ) {
                // check whether section defines classical gate
                ql::gate *firstInstr = *section->begin();
                auto firstInstrType = firstInstr->type();
                if(firstInstrType == __classical_gate__) {
                    if(section->size() != 1) {
                        FATAL("Inconsistency detected: classical gate with parallel sections");
                    }
                    classical_bundle = true;
                    ssinst << classical_instruction2cccode(firstInstr);
                } else {
                    /* iterate over all instructions in section.
                     * NB: strategy differs from cc_light_eqasm_compiler, we have no special treatment of first instruction,
                     * and don't require all instructions to be identical
                     */
                    for(auto insIt = section->begin(); insIt != section->end(); ++insIt) {
                        ql::gate *instr = *insIt;
                        auto itype = instr->type();
                        std::string iname = instr->name;
                        std::string instr_name = platform.get_instruction_name(iname);

                        if(itype == __nop_gate__)       // a quantum "nop", see gate.h
                        {
                            //FIXME: does a __nop_gate__ ever get a cc_light_instr (which are defined in JSON)?
                            ssinst << instr_name;
                        } else if(itype == __classical_gate__) {
                            FATAL("Inconsistency detected: classical gate found after first section");
                        // FIXME: do we need to test for other itype?
                        } else {    // 'normal' gate
                            auto nOperands = instr->operands.size();

                            if(1 == nOperands) {
                                auto &op0 = instr->operands[0];
                                if(verbose) emit(ssinst, SS2S("# " << instr_name << " " << op0).c_str());
//                                squbits.push_back(op);

                                /* we may have an "x" on 0:
                                   - that implies an "x' on AWGx channelGroup y
                                   - and an enable on VSM channel z
                                */


                            } else if(2 == nOperands) {
                                auto &op0 = instr->operands[0];
                                auto &op1 = instr->operands[1];
                                if(verbose) emit(ssinst, SS2S("# " << instr_name << " " << op0 << "," << op1).c_str());
//                                dqubits.push_back(qubit_pair_t(op1, op2));
                            } else {
                                FATAL("Only 1 and 2 operand instructions are supported !");
                            }
                        }


#if 0   // FIXME: cc_light SMIS/SMIT handling
                        std::string rname;
                        if( 1 == nOperands )
                        {
                            rname = gMaskManager.getRegName(squbits);
                        }
                        else if( 2 == nOperands )
                        {
                            rname = gMaskManager.getRegName(dqubits);
                        }
                        else
                        {
                            throw ql::exception("Error : only 1 and 2 operand instructions are supported by cc light masks !",false);
                        }
                        ssinst << instr_name << " " << rname;
#endif
                    }


#if 0
                    if(std::next(section) != bundle.parallel_sections.end()) {
                        ssinst << " | ";
                    }
#endif
                }
            }


            // generate bundle trailer
#if 0   // insert qwaits
            if(classical_bundle)
            {
                if(iname == "fmr")  // FIXME: this is cc_light instruction
                {
                    // based on cclight requirements (section 4.7 eqasm manual),
                    // two extra instructions need to be added between meas and fmr
                    if(delta > 2)
                    {
                        ssbundles << "    qwait " << 1 << "\n";
                        ssbundles << "    qwait " << delta-1 << "\n";
                    }
                    else
                    {
                        ssbundles << "    qwait " << 1 << "\n";
                        ssbundles << "    qwait " << 1 << "\n";
                    }
                }
                else
                {
                    if(delta > 1)
                        ssbundles << "    qwait " << delta << "\n";
                }
                ssbundles << "    " << ssinst.str() << "\n";
            }
            else
#endif
            {
                ssbundles << sspre.str() << ssinst.str() << "\n";
            }

            curr_cycle += delta;
        }

#if 0   // FIXME: CC-light
        auto &lastBundle = bundles.back();
        int lbduration = lastBundle.duration_in_cycles;
        if(lbduration > 1)
            ssbundles << "    qwait " << lbduration << "\n";
#endif

        IOUT("Generating CCCODE for bundles [Done]");
        return ssbundles.str();
    }
}; // class

} // arch
} // ql

#endif // QL_ARCH_CC_EQASM_BACKEND_CC_H

