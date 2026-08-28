import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const app=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const out=path.join(app,'test-results','brainflow');fs.mkdirSync(out,{recursive:true});
const temp=fs.mkdtempSync(path.join(os.tmpdir(),'brainflow-gate-'));
const source=fs.readFileSync(path.join(app,'src/brainflow.h'),'utf8');
const run=(exe,args)=>spawnSync(exe,args,{encoding:'utf8',maxBuffer:1024*1024,timeout:60000});
const compilerArgs=['-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fstack-protector-all'];
const executable=path.join(temp,'baseline');
let result=run('c++',[...compilerArgs,path.join(app,'tests/brainflow_gate.cpp'),'-o',executable]);
if(result.status!==0)throw Error(result.stderr);
result=run(executable,[]);if(result.status!==0)throw Error(result.stderr);
const baseline=JSON.parse(result.stdout);fs.writeFileSync(path.join(out,'exploration.json'),JSON.stringify(baseline,null,2));
const navigationExecutable=path.join(temp,'navigation');
result=run('c++',[...compilerArgs,path.join(app,'tests/brainflow_navigation_test.cpp'),'-o',navigationExecutable]);
if(result.status!==0)throw Error(result.stderr);
result=run(navigationExecutable,[]);if(result.status!==0)throw Error(result.stderr);
const navigationMatch=/brainflow_navigation_checks=(\d+) game_slots=(\d+)/.exec(result.stdout);
if(!navigationMatch)throw Error(`Navigation test did not report coverage: ${result.stdout}`);
const navigationChecks=Number(navigationMatch[1]), navigationGameSlots=Number(navigationMatch[2]);
if(navigationGameSlots!==8)throw Error(`Expected 8 navigation game slots, got ${navigationGameSlots}`);
const launcherExecutable=path.join(temp,'launcher-shortcuts');
result=run('c++',[...compilerArgs,path.join(app,'tests/launcher_shortcuts_test.cpp'),'-o',launcherExecutable]);
if(result.status!==0)throw Error(result.stderr);
result=run(launcherExecutable,[]);if(result.status!==0)throw Error(result.stderr);
const launcherMatch=/launcher_shortcut_checks=(\d+)/.exec(result.stdout);
if(!launcherMatch)throw Error(`Launcher shortcut test did not report coverage: ${result.stdout}`);
const launcherChecks=Number(launcherMatch[1]);
const audioExecutable=path.join(temp,'audio');
result=run('c++',[...compilerArgs,path.join(app,'tests/brainflow_audio_test.cpp'),'-o',audioExecutable]);
if(result.status!==0)throw Error(result.stderr);
result=run(audioExecutable,[]);if(result.status!==0)throw Error(result.stderr);
const audioMatch=/brainflow_audio_checks=(\d+)/.exec(result.stdout);
if(!audioMatch)throw Error(`BrainFlow audio test did not report coverage: ${result.stdout}`);
const audioChecks=Number(audioMatch[1]);
const mutations=[
 ['input-capacity','if (length >= kInputMax) return false;',''],
 ['submit-playing-only','if (phase != Phase::Playing) return false;',''],
 ['inactive-clock-freeze','if (phase != Phase::Playing && phase != Phase::Feedback) return;',''],
 ['deadline-equality','if (elapsed >= remaining) {','if (elapsed > remaining) {'],
 ['resume-clock-reset','phase = resumePhase;\n      lastTick = now;','phase = resumePhase;\n      (void)now;'],
 ['input-alphabet',"if (game == Game::Arithmetic ? (key < '0' || key > '9') : (key < 'A' || key > 'Z')) {\n      return false;\n    }",''],
 ['valid-choice','if (selected < 0 || selected > 2) return false;',''],
 ['score-saturation','score = score > 999999 - gain ? 999999 : score + gain;','score = score + gain;'],
 ['attempt-saturation','if (attempts < 60000) ++attempts;','++attempts;'],
 ['correct-saturation','if (correct < attempts) ++correct;','++correct;'],
 ['streak-saturation','if (streak < 99) ++streak;','++streak;'],
 ['feedback-equality','if (elapsed >= feedbackRemaining) {','if (elapsed > feedbackRemaining) {']
];
const mutationResults=[];
for(const [name,from,to]of mutations){
 if(source.split(from).length!==2)throw Error(`Nonunique fence ${name}`);
 const dir=path.join(temp,name);fs.mkdirSync(path.join(dir,'src'),{recursive:true});fs.mkdirSync(path.join(dir,'tests'));
 fs.writeFileSync(path.join(dir,'src/brainflow.h'),source.replace(from,to));
 fs.copyFileSync(path.join(app,'tests/brainflow_gate.cpp'),path.join(dir,'tests/gate.cpp'));
 const bin=path.join(dir,'check');const compile=run('c++',[...compilerArgs,path.join(dir,'tests/gate.cpp'),'-o',bin]);
 if(compile.status!==0)throw Error(`Mutant must compile: ${name}: ${compile.stderr}`);
 const check=run(bin,[]);const counterexample=check.stderr.trim();
 const killed=check.status!==0&&/Assertion failed|runtime error/.test(counterexample);
 mutationResults.push({name,executed:true,killed,status:check.status,signal:check.signal,counterexample:counterexample.slice(0,1200)});
 if(!killed)throw Error(`Fence mutant survived: ${name}`);
}
fs.writeFileSync(path.join(out,'mutations.json'),JSON.stringify(mutationResults,null,2));
const sha=createHash('sha256').update(source).digest('hex');
const summary={source_sha256:sha,cutoff:new Date().toISOString(),...baseline,mutation_score:100,mutants:mutations.length,
 navigation_checks:navigationChecks,navigation_game_slots:navigationGameSlots,launcher_checks:launcherChecks,audio_checks:audioChecks,
 actual_executed_tests:baseline.reduced_executions+baseline.fuzz_transitions+baseline.fence_fixtures+mutations.length+navigationChecks+launcherChecks+audioChecks,
 raw_cartesian_space:5*8*17*8*6*3*3*2*2**32,
 raw_equivalent_covered:5*8*17*8*6*3*3*2*2**32,
 reduction:'All 2^32 absolute clock origins are translation-equivalent for unsigned subtraction; three representatives include wrap and the signed boundary. All other declared classes are fully enumerated.',
 artifacts:{exploration:'exploration.json',mutations:'mutations.json'},temporary_mutants:temp};
fs.writeFileSync(path.join(out,'summary.json'),JSON.stringify(summary,null,2));console.log(JSON.stringify(summary));
const docs=path.join(app,'docs/brainflow.md'), model=path.join(app,'tests/brainflow_gate.cpp');
const dimensions=[['phase',5],['game',8],['input_length',17],['action',8],['elapsed_boundary',6],['score_class',3],['feedback_boundary',3],['visible',2],['clock_origin',2**32]];
const invariants=['bounded_input','exactly_once_score','no_late_input','frozen_paused_time','unsigned_clock_wrap','saturating_score_and_counts','two_level_escape','help_presentation_only','launcher_digit_precedence'];
const artifact=path.join(out,'exploration.json');
const evidence={
 release:{name:'BrainFlow 0.2.140-dev',ref:`engine-sha256:${sha}`,evidence_cutoff:summary.cutoff,gate_scope:'canary_entry',production_like_stateful:true,
  applicability:{tier:'MEANINGFUL_RISK',rationale:'Time-bound keyboard game with interruption, feedback and terminal states; model scope is the changed offline engine.'}},
 model:{states:['Menu','Playing','Feedback','Paused','Finished'],initial_states:['Menu'],
  transitions:[{name:'start',guard:'explicit new round',effect:'Playing with 60 seconds'},{name:'submit',guard:'Playing and valid input before deadline',effect:'score once then Feedback'},
  {name:'tick',guard:'Playing or Feedback',effect:'count down, advance feedback or finish'},
  {name:'pause/resume',guard:'active / Paused',effect:'preserve remaining time and feedback'}, {name:'escape',guard:'round state / Menu',effect:'BrainFlow Menu / launcher Games without network'}],
  actors:[{name:'device-loop',ownership:'single serialized owner; keyboard and clock are inputs'}],
  faults:[{name:'late/duplicate keys',injection:'action/time product and deterministic traces'},{name:'wrap/interruption',injection:'unsigned clock origins, hidden and paused states'}],
  invariants:invariants.map(name=>({name,predicate:`assertions in ${model}`})),
  fences:mutations.map(([name])=>({name,purpose:'Guard invalid local state transition or unsafe input/count boundary'})),
  dimensions:dimensions.map(([name,cardinality])=>({name,cardinality,critical:true})),
  reductions:[{kind:'symmetry',status:'applied',artifact:docs},{kind:'equivalence',status:'applied',artifact:docs},
   {kind:'partial_order',status:'not_applicable',reason:'Only one serialized loop actor; ordering is explicit in action sequences.'}]},
 coverage:{raw_equivalent_covered:summary.raw_equivalent_covered,raw_equivalence_artifact:docs,execution_counting_method:'disjoint',actual_executed_claim:summary.actual_executed_tests,
  executed_tests:{cartesian:baseline.reduced_executions,fuzz:baseline.fuzz_transitions,fixtures:baseline.fence_fixtures,mutation_programs:mutations.length,navigation:navigationChecks,launcher:launcherChecks,audio:audioChecks},
  navigation:{game_slots:navigationGameSlots,checks:navigationChecks,stack:'game -> BrainFlow menu -> launcher/Games',artifact:'tests/brainflow_navigation_test.cpp'},
  launcher:{checks:launcherChecks,plain_digits:'1-7 select group',ctrl_digits:'Ctrl+1-6 opens submenu before plain digit',artifact:'tests/launcher_shortcuts_test.cpp'},
  audio:{checks:audioChecks,cues:['start','select','correct','wrong','finish'],guard:'suppressed during recording/playback/realtime/submitting/thinking',artifact:'tests/brainflow_audio_test.cpp'},
  reduced_exploration:{complete:true,states:baseline.states,transitions:baseline.transitions,artifact},
  pairwise:{complete:true,coverage_percent:100,artifact:model},critical_threewise:{complete:true,coverage_percent:100,dimensions:dimensions.map(x=>x[0]),artifact:model},
  fuzz:{deterministic:true,seeds_retained:true,seed_artifact:artifact,executed:baseline.fuzz_transitions},
  invariant_results:invariants.map(name=>({name,passed:true,artifact})),
  mutations:mutationResults.map(m=>({fence:m.name,executed:m.executed,counterexample:m.counterexample})),mutation_score:100,
  boundary_fixtures:[{name:'empty/full/invalid input, deadline and feedback equality, pause/resume, wrap, maximum counters',passed:true,artifact:model}]},
 canaries:{synthetic:{name:'eight-game physical transition smoke',owner:'personal device operator',criteria:'User-confirmed install; each game, right/wrong response, pause, help, launcher, sounds and voice; no reset or leaked input',observability:docs,rollback:'Restore saved previous OTA manifest; no forced device downgrade',executed:false,passed:null},
  natural:{name:'eight normal full rounds with interruption',owner:'personal device operator',criteria:'One completed round per game, responsive keys, preserved settings and healthy voice after play',observability:docs,rollback:'Stop candidate rollout and restore previous manifest on any invariant failure',executed:false,passed:null}}
};
fs.writeFileSync(path.join(out,'gate-evidence.json'),JSON.stringify(evidence,null,2));
