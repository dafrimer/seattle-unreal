---
id: research-2026-10-02-amb3r-video-to-world
class: ledger
status: accepted
owner: "@dafrimer"
updated: 2026-10-03
date: 2026-10-02
---

# AMB3R ("Amb3r-SAM") for video → persistent Seattle street world

Retrieved 2026-10-02. Every factual claim cites the Sources list. Anything not read directly in a source is marked **[INFERENCE]**.

## TL;DR

- **Identity:** There is no project called "Amb3r-SAM". The closest match is **AMB3R** (Wang & Agapito, UCL; CVPR 2026 Highlight), a feed-forward model for metric-scale 3D reconstruction. It ships with **AMB3R-VO** and **AMB3R-SfM** pipelines, and a follow-up paper, **AMB3R-SLAM**, was posted on 2026-09-17 [1][3][4][25]. "SAM" is most likely a garbled "SfM"/"SLAM" **[INFERENCE]**. AMB3R contains no Segment-Anything component; its only segmentation is a SegFormer-B0 sky mask [12].
- **What it gives us:** from a folder of video frames, with no intrinsics needed, it produces per-frame camera poses, dense pointmaps, a confidence map, a PLY point cloud, and a metric-scale estimate [2][8][9][13]. It does not produce meshes, semantic labels, or a COLMAP export [8][9][19].
- **License blocker:** the repo has **no LICENSE file**; GitHub reports `license: null` [5]. The author says the weights will "likely be under a non-commercial license" because they are built on non-commercial VGGT weights and non-commercial training data. A commercial model is described as possibly impossible [17]. Treat it as **research-only, non-commercial**, which is the same class as our existing DUSt3R dependency [41][L2].
- **24 GB VRAM:** the authors' runtime figures come from an RTX 4090, a 24 GB card [2][51]. Peak VRAM for AMB3R/VO/SfM is **not published**. One user hit OOM on an RTX 4070 Ti at the default VO initialisation, and the author says memory does not grow with sequence length [18]. AMB3R-SLAM reports a 10–14 GB peak, but its code is not released [26][28]. A 3090 Ti is therefore **likely feasible but unverified** **[INFERENCE]**.
- **Recommendation: PILOT.** Run a 1–2 day, research-only test of AMB3R-SfM on our own 1–2 minute walking video at Fifth/University, with explicit stop conditions (§7). The goal is to replace DUSt3R plus sparse registration as the pose and dense-init source for gsplat. Unreal meshing and SAM masking are separate add-on tools.

---

## 1. Identity

**Resolution:** "Amb3r-SAM" is taken to mean the **AMB3R family**. The confidence is high on the family and medium on the exact sub-tool, because "SAM" is most plausibly "SfM". The user's own hint, `sfm/run.py --data_path VIDEO_DIR`, is the AMB3R-SfM entry point [4]. No repository named AMB3R-SAM or AMB3R+SAM exists:

- A GitHub repository search for `amb3r` returns 8 repos, and only `HengyiWang/amb3r` and `HengyiWang/amb3r-slam` are relevant [29].
- A search for `amb3r sam` returns 0 [29].

| Candidate | What it is | Match |
|---|---|---|
| **AMB3R** (base + AMB3R-VO + AMB3R-SfM) | Feed-forward metric-scale multi-view reconstruction with a sparse-voxel backend. It extends to uncalibrated VO and large-scale SfM without fine-tuning or test-time optimisation [1][3]. | **Best match** (covered in depth below) |
| **AMB3R-SLAM** | Same authors. Real-time monocular SLAM over kilometre-scale trajectories and 10k+ frames, with a hierarchical backend and loop closure, and "dynamic scenes out of the box" [25][26]. | Strong related candidate. The repo holds only a README with an initial commit on 2026-09-17, so **no code yet** [28]. |
| **MegaSaM** | "Accurate, Fast and Robust Casual Structure and Motion from Casual Dynamic Videos" (Google). AMB3R cites it as a dynamic-video baseline [2][40]. | Name-collision candidate ("SaM" = structure and motion). Code is Apache-2.0 [40]. |
| **Meta SAM 2 / SAM 3 / SAM 3D** | Promptable segmentation and tracking in images and video (SAM 3) [36]. SAM 3D covers object and human mesh reconstruction [39]. | Not combined with AMB3R anywhere we found [29]. Only useful as a separate masking tool (§5c). |

**AMB3R facts**

- **Authors:** Hengyi Wang and Lourdes Agapito, Department of Computer Science, UCL [1][3].
- **Paper:** arXiv:2511.20343, v1 dated 2025-11-25 [1].
- **Venue:** CVPR 2026 Highlight [3][4].
- **Links:** project page https://hengyiwang.github.io/projects/amber [3]; repo https://github.com/HengyiWang/amb3r [4].
- **Releases and activity:**
  - The repo has no releases or tags [5][6].
  - The base model, VO, and SfM code were released 2026-02-21/24 [4].
  - The benchmark code followed on 2026-02-28 [4].
  - Training code landed 2026-03-09, and metric-head training on 2026-05-08 [4].
  - VGGT-Omega and Déjà View plug-ins were added 2026-05-16 and 2026-06-05 [4].
  - The last push was 2026-06-05 ("support deja view") [6].
  - At retrieval the repo had 503 stars, 36 forks, and 13 open issues [5].
- **AMB3R-SLAM:** arXiv:2609.19518, dated 2026-09-17 [25]. Its project page is at hengyiwang.github.io/projects/amber-slam [27].

## 2. What it does

### Architecture

- **Front-end:** a frozen VGGT that predicts pointmaps, depth, and camera, plus a lightweight **metric scale head** [2 §1, §4.1].
- **Backend:** a sparse-voxel backend (Point Transformer v3 over space-filling-curve-serialised voxels). Its features are fused back into the frozen front-end through zero-convolution [2 §4.2][3].
- **Training:** about 80 H100 GPU-hours for the backend plus scale head [2 §1].

### Inputs

- **Input format:** a folder of image files, not a video file.
  - The SfM/base loader accepts `.jpg/.jpeg/.png/.heic/.ppm` [14].
  - The VO loader accepts `.png/.jpg/.jpeg/.bmp/.tiff` [15].
  - **Frames must be extracted from video first** **[INFERENCE]**.
- **Intrinsics:** not required. The loaders insert pseudo-intrinsics (fx = fy = 1) [14]. Known intrinsics are **not supported** for SfM; the author suggests Umeyama alignment afterwards if you have known poses [21].
- **Resolution:** all demo entry points resize/crop to **518×392** [8][9][14][15]. The VO runtime figures use 392×518 [2 §8.5].
- **Frame counts:**
  - Base model: compute is quadratic in the number of images, like other global-attention models [2 §7.1]. The demo caps the base model at 4 images (`max_images=4`) [57].
  - VO: runs over keyframe windows (`map_init_window: 20`, `map_every: 8`, `max_keyframes: 10`) [10].
  - SfM: clusters frames, with clusters of at most 8 and at most 10 mapping frames [11]. It was evaluated on Tanks & Temples scenes of **151–1106 images**, which come from video [2 Tab. 13, §8.7].

### Outputs

- **VO and SfM (`run.py`) write two files:**
  - A **PLY point cloud**, downsampled to 3M points for SfM and 6M for VO [8][9].
  - An `.npz` containing per-frame `pts` (dense pointmaps), `conf`, `pose` (4×4 c2w), `images`, `sky_mask`, and `kf_idx`. The SfM file also contains `unmapped_frames` [8][9].
- **Per-frame intrinsics** are computed inside the front-end (`predictions['intrinsic']`) but **not saved** by `run.py` [13][8][9].
- **Metric scale:**
  - The scale head predicts the per-frame metric log-depth of the median-depth pixel. The median across frames then aligns the reconstruction to metric space [2 §4.1][13].
  - The VO/SfM outputs **are not metric by default**. You must apply `metric_scale_median` yourself to the points and to the camera translations [20].
- **Not produced:** meshes, semantic labels or masks (beyond sky), and COLMAP files. A request for a COLMAP export for 3DGS is open with no reply [19].

### Modes

1. **Feed-forward base model:** `demo.py`, with an interactive viewer and a backend on/off toggle [4].
2. **AMB3R-VO (online):**
   - Keyframes act as memory.
   - Relative scale per window comes from a robust (ROE) solver.
   - Confidence-weighted fusion.
   - No loop closure [2 §4.4, §7.2].
3. **AMB3R-SfM (offline):** image clustering, then coarse incremental registration, then global mapping. No bundle adjustment [2 §4.5, §7.3][3].
4. **AMB3R-SLAM (paper only):**
   - Front-end: DA3-Small (80M parameters).
   - Hierarchical Sim(3) pose graph with span-2 edges, long-context edges, and DBoW2 loop closure.
   - Optional stereo, RGB-D, or LiDAR input [26 §3].

The VO and SfM pipelines are **model-agnostic**. Any model returning pointmap, pose, and confidence can be plugged in [4][24]. `model_zoo.py` already wraps DA3 and VGGT-Omega [56].

### Segmentation component

- The only segmentation is **SegFormer-B0 (ADE20K, 512×512)**, used to mask **sky** (class index 2) before point filtering [12].
- Its checkpoint (45.3 MB) is committed in `checkpoints/` [16].
- AMB3R has no SAM, SAM 2, or SAM 3 component [4][12].

### Dynamic objects (cars, pedestrians)

- **AMB3R:** "not trained for dynamic scenarios… heavily relies on static cues… might fail when the target scene is dominated by dynamic objects" [2 §7.1]. Even so, its raw VO is reported as comparable to MegaSaM on TUM dynamic sequences [2 §5.7].
- **AMB3R-SLAM:** claims dynamic robustness by avoiding bundle adjustment. Its pose graph uses relative poses only, with no explicit motion segmentation [26 §3.3].
- **Neither removes moving objects from the reconstruction** **[INFERENCE]**. Cars and pedestrians will appear as points or ghosts.

## 3. Practicalities

### Licenses

| Component | License | Commercial? |
|---|---|---|
| AMB3R code (repo) | **None.** No LICENSE file; GitHub API `license: null` [5]. The author's plan is a permissive license such as Apache-2.0 for the VO/SfM pipelines, "might need some time" [17]. | **Unknown. Currently no grant.** With no license, default copyright applies, so there is no permission to reuse or redistribute **[INFERENCE]**. |
| AMB3R weights (`amb3r.pt`) | Not stated. The author says "likely… non-commercial" because they are built on the non-commercial VGGT weights [17]. A commercial variant "might not be possible" because most training datasets are non-commercial [17]. | **No (expected non-commercial)** |
| VGGT front-end weights inside AMB3R | VGGT-1B is CC-BY-NC-4.0 [32]. Only the gated VGGT-1B-Commercial checkpoint allows commercial use [30][33]. The VGGT code license is commercial-friendly (VGGT License) [30][31]. | No (AMB3R uses the NC weights [17]) |
| SegFormer sky model | NVIDIA Source Code License: "may be used… non-commercially" [34] | No |
| AMB3R-SLAM | No code. The repo has no license [28]. | Unknown |

Our current pipeline already depends on DUSt3R, which is CC BY-NC-SA 4.0 [41][L2]. So AMB3R does not make our licensing position worse for research use. It does **not** open a commercial path.

### Dependencies and environment (from README/requirements)

- **Core stack** [4][7]:
  - conda env with Python **3.9** and cmake 3.14.0
  - **torch 2.5.0 / torchvision 0.20.0 with CUDA 11.8 wheels**
  - torch-scatter 2.1.2
  - **pytorch3d v0.7.8 built from git**
  - **flash-attn 2.7.3**, built with `--no-build-isolation`
- **Other packages** [7]: xformers 0.0.28.post2, spconv-cu118 2.3.8, open3d 0.18.0, utils3d from git, and others.
- **flash-attn:**
  - FlashAttention-2 supports Ampere GPUs, including the RTX 3090 [50], so our **SM 8.6 3090 Ti is supported** [50].
  - Upstream marks Linux as the supported OS and Windows as "might work" [50].
  - Current upstream docs list CUDA ≥12.0, which may complicate building the pinned 2.7.3 against cu118 **[INFERENCE]**.
- **Linux/WSL:**
  - No Windows or WSL instructions are given [4].
  - The stack is Linux-native, so **WSL2 Ubuntu 22.04 is the viable target**, using a separate conda env because our environments are Python 3.10 **[INFERENCE]**.
  - A past clone failure from stale Git LFS assets was fixed by the author [58].
- **Weights:**
  - `amb3r.pt` is hosted on **Google Drive** (manual download into `./checkpoints/`) [4][54].
  - Its size is not shown on the Drive page without downloading [54]. It embeds the frozen VGGT front-end, and the VGGT-1B fp32 checkpoint alone is **5.03 GB** on Hugging Face [32], so expect ≥5 GB **[INFERENCE]**.
  - SegFormer-B0 (45.3 MB) ships in the repo [16].

### VRAM, RAM, and runtime

| Item | Value | Source |
|---|---|---|
| AMB3R-VO speed | 4.2 FPS average (3.4–6.0) on an **RTX 4090** at 392×518, excluding data loading; >10 FPS at reduced resolution | [2 §8.5] |
| RTX 4090 memory | 24 GB, the same capacity as our 3090 Ti | [51] |
| AMB3R / VO / SfM peak VRAM | **Not published** | [2][4] |
| VO OOM report | RTX 4070 Ti OOMs at VO initialisation. Fix: reduce `map_init_window` (default 20), then `map_every`. Memory "will not grow with respect to sequence length". | [18][10] |
| SfM memory anecdote | A user reports "very low GPU memory usage" on long sequences (not quantified) | [22] |
| AMB3R-SLAM | 10.2–17.6 FPS mono on a 4090; **peak 10–14 GB**, not growing with sequence length | [26 §4.3] |
| Host RAM | `run.py` loads **all frames into a single tensor** and keeps per-frame dense pointmaps [8][9]. At 518×392 float32 that is about 2.4 MB per frame for images plus about 2.4 MB per frame for points, so roughly 6 MB per frame with confidence. 3,600 frames (2 min at 30 fps) is about 20 GB, too much for our ~32 GB WSL; 300 frames is about 1.8 GB. **Subsample to 2–3 fps.** | [INFERENCE from 8][9] |

**24 GB feasibility verdict:** likely feasible for VO and SfM at the default 518×392, because the authors' own VO numbers come from a 24 GB card [2][51]. This is **unverified**: no published peak figure exists, and measuring it is pilot step 1.

## 4. Evidence of quality (outdoor / street / metric)

**Metric multi-view depth** (RMVDB; rel %↓ / δ1.03↑ / δ1.25↑) [2 Tab. 4]. The AMB3R row is the log-depth "enc. z" variant, which the paper says is better overall [2 §5.4]:

| Method | KITTI | ETH3D | Tanks & Temples |
|---|---|---|---|
| **AMB3R** | 8.2 / 27.9 / 95.6 | 8.5 / 48.3 / 90.4 | 6.3 / 49.9 / 95.7 |
| MapAnything | 12.8 / 16.1 / 78.2 | 15.4 / 26.6 / 83.1 | 24.6 / 15.3 / 41.5 |
| CUT3R | 28.6 / 4.1 / 29.5 | 29.4 / 6.4 / 40.7 | 46.7 / 0.1 / 1.9 |
| MASt3R | 56.8 / 0.1 / 8.4 | 40.4 / 10.3 / 26.7 | 63.7 / 0.0 / 1.5 |

**Other results**

- **Video depth on KITTI** (scale-aligned) [2 Tab. 6]: AMB3R rel 4.1, δ1.25 98.3, against VGGT 8.2 / 90.9 and π3 3.6 / 98.8.
- **Monocular depth on KITTI** [2 Tab. 1]: AMB3R 7.3 / 95.4, against VGGT 8.8 / 92.7 and MoGe 5.5 / 97.6.
- **Outdoor multi-view reconstruction on ETH3D** [2 Tab. 5]: AMB3R rel 4.64, Acc 9.98 cm, Comp 9.69 cm. VGGT scores 6.02 / 12.81 / 11.89, and π3 scores 5.82 / 10.54 / 10.42.
- **SfM, no bundle adjustment:**
  - ETH3D average RRA@5 / RTA@5: **98.2 / 81.9**, against COLMAP 49.0 / 47.8 and MASt3R-SfM 81.2 / 79.7. The "facade" scene scores 100 / 95.4 [2 Tab. 12].
  - Tanks & Temples Training / Intermediate / Advanced: 95.0/94.5, 98.7/96.9, and 68.0/72.4 [2 Tab. 13].
- **Long driving (KITTI 00–10, 0.4–5.1 km)** [26 Tab. 2]:
  - **AMB3R-VO** has an average ATE of **104.04 m**, so it drifts badly at kilometre scale.
  - **AMB3R-SLAM** monocular has an average ATE of **13.11 m**. The stereo variant gets 12.15 m and the LiDAR variant 0.95 m.
  - On Oxford Spires, AMB3R-SLAM averages **2.32 m**, against 8.59 m for LoGeR [26 Tab. 3].

**Known failure modes and limitations**

- **Scale and tracking:**
  - Scale alignment becomes unreliable, often causing tracking failure, in scenes with **diverse depth ranges and complex thin structures**. Distant content increases variance [2 §7.2]. A street canyon with poles, wires, and trees, and with views down the avenue, matches both conditions **[INFERENCE]**.
  - VO has no loop closure, so it drifts over large areas [2 §7.2].
- **SfM:**
  - Initialisation prefers indoor-looking regions [2 §7.3].
  - Pose-distance view selection can miss wide-baseline overlaps [2 §7.3].
  - Visually similar but non-overlapping clusters can be **hallucinated** with moderate confidence. This is a Doppelganger risk for repetitive facades [2 §7.3].
- **Poses for splatting:**
  - Poses are weighted-average feed-forward poses, not bundle-adjusted. This can mean **lower NVS PSNR** than COLMAP poses, and the paper recommends BA post-processing [2 §7.3].
  - On 7-Scenes, NVS PSNR was 21.8 with AMB3R-VO poses against 24.4 with COLMAP ground truth [2 Tab. 7].
- **Reproducibility and transfer:**
  - The model is not trained on dynamic scenes [2 §7.1].
  - Users could not reproduce the ETH3D (outdoor) reconstruction numbers when retraining. They saw unstable outdoor metrics driven by pose errors on "meadow" and "playground". The released weights do reproduce [23].
  - The author notes that outdoor pose error inflates relative point error [23].
- **AMB3R-SLAM:** its map is a point cloud that may show **duplicated surfaces or ghosting** [26 §4.3].

## 5. Fit for our pipelines

### (a) Replace or augment DUSt3R plus sparse registration for gsplat

**Verdict: yes, the most promising use.** This assumes our input moves from 15 panoramas (7 registered) to a perspective walking video.

**What AMB3R-SfM gives**

- Poses for every frame. With `allow_unmapped_frames: false`, unmapped frames are force-remapped [11][55].
- Dense per-pixel points and confidence, plus a sky mask [8].
- A metric scale available from the scale head [20].
- Our DUSt3R route gave 7 of 15 registered cameras and 278 sparse points [context].

**Extra tooling needed**

1. **Frame extraction** (ffmpeg or similar) into a folder, because the loaders read images only [14][15].
2. **Exporter patch:**
   - Save `predictions['intrinsic']` per frame [13].
   - Multiply points and translations by `metric_scale_median` [20].
   - Write a COLMAP model (`cameras.txt` PINHOLE, `images.txt`, `points3D.txt` from confidence- and sky-filtered points). AMB3R has no exporter [19].
   - VGGT's `demo_colmap.py`, with optional bundle adjustment, is a reference design whose output "can be directly used with gsplat" [30].
   - Our camera contract is OpenCV c2w in a COLMAP frame with PINHOLE intrinsics [L1]. AMB3R saves 4×4 c2w [8], so the conventions should line up after verification **[INFERENCE]**.
3. **Optional bundle adjustment** (COLMAP or VGGT's BA path) before gsplat, given the paper's PSNR caveat [2 §7.3][30].
4. The **gsplat trainer** we already vendor (Apache-2.0) [49].
5. Panoramas would need perspective reprojection, because every loader centre-crops to 518×392 [14][15]. Video sidesteps this **[INFERENCE]**.

**Effect on our pain points**

- Registration: dense video gives about 100× more overlap than 15 panoramas **[INFERENCE]**.
- Missing road: AMB3R recovers road wherever the camera sees it. Capture must tilt down **[INFERENCE]**.
- Metric scale: provided by the scale head [2 §4.1].
- Diffusion fill: real coverage should reduce how much ViewCrafter must hallucinate **[INFERENCE]**.

### (b) Feed the Unreal mesh and collision workflow

**Verdict: partial.** AMB3R stops at a point cloud [8][9], so meshing and collision are all extra steps.

**Needed pipeline**

1. Semantic labels per point (see (c)).
2. Metric-scale and OSM alignment. AMB3R's own frame is the first camera [2 §4.4]. A similarity or Umeyama transform to OSM footprint corners or façade lines is needed; the author already recommends Umeyama for aligning to known coordinates [21].
3. **RANSAC ground plane** on road and sidewalk points, and **façade planes** per building. Open3D is already an AMB3R dependency [7].
4. **Meshing** with Poisson or TSDF, then decimation **[INFERENCE: standard tools, not in AMB3R]**.
5. **Collision proxies:** boxes or planes from the fitted planes, imported into UE 5.8 as simple collision **[INFERENCE]**.

**Notes**

- Expect noise from dynamic objects and SLAM ghosting, so plane fitting is more robust than raw meshes [26 §4.3] **[INFERENCE]**.
- For seattle-unreal's stylized OSM meshes, the realistic use is **validation and refinement**: checking façade setbacks, sidewalk width, and curb height against OSM. It is not a source of final meshes **[INFERENCE]**.

### (c) SAM-style masks to remove people and cars and label road, sidewalk, and building

**Verdict: needs a separate tool.** AMB3R only masks sky with SegFormer-B0 ADE20K [12].

**Options**

- **Reuse the bundled SegFormer for labels.**
  - ADE20K's 150 classes include building, road, sidewalk, person, car, bus, truck, bicycle, pole, and streetlight [35].
  - AMB3R's code already selects one class index [12]. Selecting road, sidewalk, and building instead is a small code change **[INFERENCE]**.
  - B0 is the smallest SegFormer and may be coarse **[INFERENCE]**.
  - License: NVIDIA non-commercial [34].
- **SAM 3 for dynamic-object removal.**
  - Text-prompted detection, segmentation, and **video tracking**; it accepts a JPEG folder or a video [36].
  - Prompts like "person", "car", or "bicycle" would give temporally consistent masks **[INFERENCE]**.
  - The SAM License grants royalty-free use and derivatives, with trade-control and military restrictions [37].
  - SAM 2 is Apache-2.0 [38].
- **Where masks apply:**
  - AMB3R inference takes no mask input. The demo loader's `_mask.png` only masks ground-truth depth for evaluation [14] **[INFERENCE]**.
  - Apply masks **after** inference: drop masked pixels from the point export, the COLMAP `points3D`, and the gsplat photometric loss.
  - Our repo already has an `experiments/mask-pilot/` directory [L3].
- **Caveat:** masking does not fix pose errors caused by dynamic objects. AMB3R relies on static cues [2 §7.1].

## 6. Alternatives

Only facts stated in the primary sources are included. "n/s" means not stated in the sources consulted.

| Model | Inputs | Metric scale | Long video handling | License (code / weights) | VRAM |
|---|---|---|---|---|---|
| **AMB3R** (VO/SfM) | Image folder, no intrinsics [14][15][21] | Yes, scale head. KITTI metric depth rel 8.2% [2] | VO bounded memory; SfM tested on 151–1106 images [2][18]. VO drifts at km scale (KITTI ATE 104 m) [26] | **None** / expected NC [5][17] | Runs on a 24 GB 4090 [2][51]. Peak n/s |
| **AMB3R-SLAM** | Mono video; optional stereo, RGB-D, LiDAR [26] | Mono is Sim(3); metric with stereo, RGB-D, or LiDAR [26 §3.4] | 10k+ frames, loop closure; KITTI mono ATE 13.11 m [26] | No code yet [28] | 10–14 GB peak on a 4090 [26] |
| **DUSt3R / MASt3R(-SfM)** (current) | Image pairs plus global alignment [2 §2.2] | MASt3R has a "metric" checkpoint [42]. KITTI metric rel 56.8 [2] | n/s | CC BY-NC-SA 4.0 [41][42] | n/s |
| **MASt3R-SLAM** | mp4 or folder; intrinsics optional via `--calib` [43] | n/s | OOM on 3 of 11 KITTI sequences in AMB3R-SLAM's benchmark [26 Tab. 2] | CC BY-NC-SA 4.0 [44] | n/s. WSL supported via `windows` branch [43] |
| **VGGT** | Image set, feed-forward [30] | No; normalised pointmaps [2 §4.1] | Limited image count [2 §2.2]. VGGT-Long KITTI ATE 25.94 m [26] | Commercial-friendly code; 1B weights CC-BY-NC, separate gated commercial checkpoint [30][32][33] | n/s (5.03 GB checkpoint [32]) |
| **CUT3R** | Streaming frames [45] | KITTI metric rel 28.6 [2] | Current implementation has linear memory in frame count [45] | CC BY-NC-SA 4.0 [45] | n/s |
| **π3 / Pi3X** | Image set; Pi3X optionally takes pose, intrinsics, depth [46] | Pi3X "approximate" metric [46] | π3-Long KITTI ATE 21.17 m [26] | BSD-3 code / **CC BY-NC 4.0 weights** [46] | n/s |
| **MapAnything** | Images plus optional geometric inputs [47] | Yes, "metric 3D reconstruction" [47]. KITTI metric rel 12.8 [2] | Memory-efficient mode up to 2000 views on 140 GB [47] | Apache-2.0 code; **weights: CC-BY-NC or an Apache-2.0 variant** [47] | n/s |
| **Depth Anything 3** | Any-view; pose and intrinsics estimation [48] | Nested-Giant has metric depth [48] | DA3-Streaming: "less than 12GB" for ultra-long video [48]. DA3-Long KITTI ATE 16.83 m [26] | Apache code; Giant/Large CC BY-NC, Small/Base/Metric-Large Apache-2.0 [48] | <12 GB (streaming) [48] |
| **MegaSaM** | Casual dynamic video [40] | n/s | n/s | Apache-2.0 code [40] | n/s |

**Takeaways for us**

- No permissively licensed weights match AMB3R's metric numbers. MapAnything-Apache and DA3-Small/Base are the only commercial-clean options listed [47][48].
- AMB3R-VO/SfM can host DA3 (`--model_name da3`), so the pipeline could later be paired with an Apache-licensed base model, once AMB3R itself has a code license [9][24][48] **[INFERENCE]**.

## 7. Recommendation: **PILOT** (time-boxed, research-only)

**Why pilot rather than go**

- No code license exists, and the weights are expected to be non-commercial [5][17].
- Peak VRAM is unpublished [2].
- Poses come without bundle adjustment [2 §7.3].
- Street canyons hit two documented failure conditions [2 §7.2].

**Why pilot rather than no-go**

- It directly targets our worst failures: registration, density, and metric scale.
- It is the strongest published outdoor SfM in its class (ETH3D 98.2 RRA@5 without BA) [2].
- It runs on a 24 GB-class GPU [2][51].

**Pre-condition:** use is internal, non-commercial evaluation only. Ask the author in issue #8 for a license before anything is redistributed [17].

### Pilot plan (RTX 3090 Ti 24 GB, WSL2 Ubuntu 22.04)

**Data capture**

- **Source:** our own phone or action-cam video. We own the copyright, which avoids Mapillary's CC-BY-SA share-alike on derivatives [52] and Panoramax's per-instance picture licenses (etalab-2.0 or CC-BY-SA-4.0 advised) [53].
- **Privacy:** blur faces and plates before publishing any splat **[INFERENCE: privacy practice, not legal advice]**.
- **Route:** 1–2 minutes at Fifth Ave / University St. Walk one block-face on the sidewalk, cross legally at the crosswalk, and return along the opposite sidewalk so the path closes a loop. VO has no loop closure, but SfM and global mapping use all frames [2 §4.5].
- **Camera settings:** landscape orientation to match the 518×392 aspect [8][9]. Lock exposure and focus if the app allows, and turn off any wide or fisheye mode **[INFERENCE]**.
- **Framing:** pitch about 10–20° down so road and curb fill the lower third. Do several deliberate pans toward each façade, and avoid long shots straight down the avenue, because distant-dominated views hurt scale [2 §7.2].
- **Pace and timing:** walk slowly (about 1 m/s) and avoid fast rotations. Go at a low-traffic hour; the model relies on static cues [2 §7.1].
- **Scale reference:** tape-measure one reference, such as sidewalk width or a crosswalk stripe, and record which one.

**Steps**

1. **Environment:** create a separate conda env in WSL (py3.9, torch 2.5.0+cu118, pytorch3d, flash-attn 2.7.3, spconv-cu118) following [4][7]. Download `amb3r.pt` [54]. Time-box this to half a day.
2. **Frames:** extract at 2–3 fps, giving 240–360 frames for 2 minutes, which keeps host RAM to about 2 GB (§3). Keep every 8th frame as **holdout** for gsplat evaluation.
3. **Run AMB3R-SfM:** `sfm/run.py --data_path frames/` [4]. Log peak VRAM (`nvidia-smi`) and wall time.
   - Optionally also run `slam/run.py`, lowering `map_init_window` if it OOMs [18].
4. **Export patch** (our code, not upstream):
   - Save the per-frame `intrinsic` [13].
   - Apply `metric_scale_median` [20].
   - Write a COLMAP text model following VGGT `demo_colmap.py` [30].
   - Produce one variant without BA and one with BA.
5. **Masks:** run SAM 3 video with the prompts person, car, bus, truck, bicycle [36], and SegFormer road/sidewalk/building labels [12][35]. Drop dynamic pixels from `points3D` and from the gsplat loss.
6. **gsplat:** train with our vendored gsplat [49] from the AMB3R init. Evaluate the holdout frames.
7. **Geometry probe for Unreal:** RANSAC the road/sidewalk plane and the façade planes; compare with OSM footprints in seattle-unreal.

**Success metrics** (thresholds are our proposals, not from sources)

| Metric | Target |
|---|---|
| Registered-frame fraction (frames not in `unmapped_frames` [8] and with median conf above threshold) | ≥ 90% (baseline: 7/15 ≈ 47%) |
| Scale error: façade-to-façade distance across Fifth Ave vs OSM building footprints, plus the tape-measured reference | ≤ 5% |
| Coverage: share of 0.25 m BEV cells inside the OSM street plus sidewalk polygon containing ≥1 road or sidewalk point; share of façade length with points | ≥ 70% road, ≥ 80% façades |
| Splat holdout PSNR (every 8th frame) | ≥ 22 dB and ≥ current `seattle-support-*` best + 3 dB |
| Peak VRAM / wall time | ≤ 22 GB / ≤ 30 min for SfM on ≤ 360 frames |

**Stop conditions**

- The environment cannot be built in WSL within 1 day.
- OOM persists at default settings and with reduced `map_init_window`.
- Registered fraction is < 70%, or doubled or "hallucinated" façades appear [2 §7.3].
- Scale error is > 10% after applying `metric_scale_median`.
- Holdout PSNR shows no gain over the current pipeline even with BA.
- The license is confirmed incompatible with intended use.

**If AMB3R-SLAM code is released:** re-run the same capture through it. It adds loop closure and dynamic robustness [26].

---

## 8. AMB3R-SLAM paper (arXiv 2609.19518v1): what it changes for us

Read in full 2026-10-03 [26]. The code repo still contains only `.gitignore` and a 12-byte README, so none of this is runnable yet [28].

**System**

- **Front-end:** DA3-Small (80M), tracking each frame against an anchor keyframe plus the two latest frames.
- **Backend:** a "giant" DA3 model, or VGGT-Ω as the variant marked (Ω), builds overlapping submaps. A Sim(3) pose graph joins them through three kinds of edges:
  - span-2 edges
  - sparse long-context edges
  - DBoW2 loop closures, checked geometrically by voxel overlap
- **No bundle adjustment.** The pose graph runs on relative poses only, which the authors give as the reason moving objects are handled without motion segmentation [26 §3.1–3.3].

**Runtime and memory (RTX 4090)** [26 Tab. 9]:

- Monocular: 10.2–17.6 FPS.
- Peak memory: 10.3–14.0 GB, which does not grow with sequence length.

This is the first published memory figure for the family. It sits comfortably inside the 3090 Ti's 24 GB **[INFERENCE: Ampere vs Ada throughput is lower, so expect lower FPS, but memory should transfer]**.

**Results relevant to a street walk**

| Benchmark | What it is | Result | Source |
|---|---|---|---|
| Oxford Spires | Urban/campus captures, 0.26–0.80 km | Avg ATE **2.32 m** (2.08 m Ω) vs 8.59 m LoGeR, 8.62 m LingBot-Map | [26 Tab. 3] |
| VBR | Rome urban, 1–5.2 km | Avg ATE **7.42 m** vs 26.65 m best online baseline | [26 Tab. 4] |
| LaMAR / CroCoDL | Building-scale, indoor and outdoor | Full-trajectory W-AUC 75.7 / 91.2; GLOMAP gets 62.3 / 76.4 | [26 Tab. 5] |
| Bonn dynamic | Moving people | ATE **1.3 cm**, best on all 8 sequences | [26 Tab. 8] |

**Ablation that sizes our capture.** Without loop closure, tracking stays reliable up to about **100 m** and then drifts; W-AUC falls to 55% at 500 m [26 Tab. 10]. A 1–2 minute walk at Fifth/University covers roughly 100–150 m **[INFERENCE]**, which is the regime where it holds without loops. Ending the walk where it started still gives the loop-closure backend something to use.

**Scale.** The paper states monocular SLAM suffers scale ambiguity and drift. Scale is fixed only with stereo, RGB-D, or LiDAR, which pins the pose graph to SE(3) [26 §3.4]. Metric scale from monocular video therefore still depends on the foundation model's own metric output plus our OSM / tape-measure check.

Phones with depth sensors could in principle feed the RGB-D path **[INFERENCE: the paper only tests TUM RGB-D and notes depth sensors give "modest" gains because of range limits [26 §4.2]; a phone sensor would not reach facades across the street]**.

**Limitations the authors state** [26 §5 Limitations]:

- The map is a point cloud, with "duplicated surfaces or ghosting artifacts in perceptually ambiguous regions". Repetitive facade windows are exactly that case for us **[INFERENCE]**.
- No mesh or surface output, so the Unreal meshing gap in §5b is unchanged.

**Licensing.** The paper's backbones are DA3-Small and DA3-Giant or VGGT-Ω [26 §4.1]. DA3 Small and Base are Apache-2.0, while Giant and Large are CC BY-NC [48]. The "giant" backend that produced the headline numbers is therefore non-commercial **[INFERENCE: matching "giant model [36]" to DA3-Giant]**. The repo license is unknown until code ships [28].

**Effect on the recommendation**

- **The pilot stands and still targets AMB3R-SfM**, the only runnable code.
- **For our use, AMB3R-SLAM is the better design** once it is released:
  - bounded, published memory
  - explicit handling of moving objects
  - loop closure
  - urban benchmarks close to our scene
- **Pilot design change:** keep the walk to about 150 m or less, end where it started, and record the same frames so the identical input can be re-run on AMB3R-SLAM when its code appears.
- **Add to the stop and decide gate:** if AMB3R-SfM fails on cars and pedestrians, wait for AMB3R-SLAM rather than investing in masking workarounds.

## Sources

1. AMB3R arXiv abstract (2511.20343): https://arxiv.org/abs/2511.20343
2. AMB3R full text (HTML v1; sections and tables cited as §/Tab.): https://arxiv.org/html/2511.20343
3. AMB3R project page: https://hengyiwang.github.io/projects/amber
4. AMB3R GitHub README: https://github.com/HengyiWang/amb3r
5. AMB3R repo metadata (license null, pushed_at, stars, no releases): https://api.github.com/repos/HengyiWang/amb3r
6. AMB3R commit history: https://github.com/HengyiWang/amb3r/commits/main
7. requirements.txt: https://github.com/HengyiWang/amb3r/blob/main/requirements.txt
8. sfm/run.py: https://github.com/HengyiWang/amb3r/blob/main/sfm/run.py
9. slam/run.py: https://github.com/HengyiWang/amb3r/blob/main/slam/run.py
10. slam/slam_config.yaml: https://github.com/HengyiWang/amb3r/blob/main/slam/slam_config.yaml
11. sfm/sfm_config.yaml: https://github.com/HengyiWang/amb3r/blob/main/sfm/sfm_config.yaml
12. amb3r/tools/pts_vis.py (SegFormer sky mask): https://github.com/HengyiWang/amb3r/blob/main/amb3r/tools/pts_vis.py
13. amb3r/frontend.py (intrinsics, metric scale): https://github.com/HengyiWang/amb3r/blob/main/amb3r/frontend.py
14. amb3r/datasets/demo.py (SfM/base loader): https://github.com/HengyiWang/amb3r/blob/main/amb3r/datasets/demo.py
15. slam/datasets/demo.py (VO loader): https://github.com/HengyiWang/amb3r/blob/main/slam/datasets/demo.py
16. checkpoints/ (segformer.b0 45.3 MB): https://github.com/HengyiWang/amb3r/tree/main/checkpoints
17. Issue #8 "License": https://github.com/HengyiWang/amb3r/issues/8
18. Issue #7 "CUDA Out of Memory on RTX 4070 Ti…": https://github.com/HengyiWang/amb3r/issues/7
19. Issue #13 "COLMAP output for 3DGS": https://github.com/HengyiWang/amb3r/issues/13
20. Issue #15 "Metric scale in AMB3R-VO and training": https://github.com/HengyiWang/amb3r/issues/15
21. Issue #11 "Running with camera known extrinsic/intrinsic parameters?": https://github.com/HengyiWang/amb3r/issues/11
22. Issue #16 "How can I visualize the result of AMB3R (SfM)?": https://github.com/HengyiWang/amb3r/issues/16
23. Issue #18 "Reproduction… ETH3D": https://github.com/HengyiWang/amb3r/issues/18
24. Issue #3 "Swapping VGGT frontend with Depth Anything V3?": https://github.com/HengyiWang/amb3r/issues/3
25. AMB3R-SLAM arXiv abstract (2609.19518): https://arxiv.org/abs/2609.19518
26. AMB3R-SLAM full text (HTML v1): https://arxiv.org/html/2609.19518
27. AMB3R-SLAM project page: https://hengyiwang.github.io/projects/amber-slam
28. AMB3R-SLAM repo (README only, initial commit 2026-09-17): https://github.com/HengyiWang/amb3r-slam
29. GitHub repository search `amb3r` / `amb3r sam`: https://api.github.com/search/repositories?q=amb3r and https://api.github.com/search/repositories?q=amb3r+sam
30. VGGT README (license note, COLMAP export): https://github.com/facebookresearch/vggt
31. VGGT LICENSE: https://github.com/facebookresearch/vggt/blob/main/LICENSE.txt
32. VGGT-1B model card / files (CC-BY-NC-4.0, model.pt 5.03 GB): https://huggingface.co/facebook/VGGT-1B
33. VGGT-1B-Commercial (gated): https://huggingface.co/facebook/VGGT-1B-Commercial
34. SegFormer LICENSE (NVIDIA Source Code License, non-commercial): https://github.com/NVlabs/SegFormer/blob/master/LICENSE
35. ADE20K class list (objectInfo150.csv): https://github.com/CSAILVision/sceneparsing/blob/master/objectInfo150.csv
36. SAM 3 README: https://github.com/facebookresearch/sam3
37. SAM 3 LICENSE (SAM License): https://github.com/facebookresearch/sam3/blob/main/LICENSE
38. SAM 2 repo (Apache-2.0): https://github.com/facebookresearch/sam2
39. SAM 3D Objects README: https://github.com/facebookresearch/sam-3d-objects
40. MegaSaM README (Apache-2.0): https://github.com/mega-sam/mega-sam
41. DUSt3R LICENSE (CC BY-NC-SA 4.0): https://github.com/naver/dust3r/blob/main/LICENSE
42. MASt3R README / LICENSE: https://github.com/naver/mast3r
43. MASt3R-SLAM README: https://github.com/rmurai0610/MASt3R-SLAM
44. MASt3R-SLAM LICENSE: https://github.com/rmurai0610/MASt3R-SLAM/blob/main/LICENSE.md
45. CUT3R README / LICENSE: https://github.com/CUT3R/CUT3R
46. π3 / Pi3X README (license table): https://github.com/yyfz/Pi3
47. MapAnything README: https://github.com/facebookresearch/map-anything
48. Depth Anything 3 README (model/license table, DA3-Streaming): https://github.com/ByteDance-Seed/Depth-Anything-3
49. gsplat (Apache-2.0): https://github.com/nerfstudio-project/gsplat
50. FlashAttention README (Ampere support, Linux): https://github.com/Dao-AILab/flash-attention
51. NVIDIA GeForce RTX 4090 (24 GB): https://www.nvidia.com/en-us/geforce/graphics-cards/40-series/rtx-4090/
52. Mapillary "CC-BY-SA license for open data": https://help.mapillary.com/hc/en-us/articles/115001770409-CC-BY-SA-license-for-open-data
53. Panoramax API settings (picture license per instance): https://gitlab.com/panoramax/server/api/-/blob/develop/docs/install/settings.md
54. AMB3R checkpoint (Google Drive, `amb3r.pt`): https://drive.google.com/file/d/14x0WW2rUE_he2hUEouP6ywSRnlJDeLel/view
55. sfm/pipeline.py (unmapped-frame handling): https://github.com/HengyiWang/amb3r/blob/main/sfm/pipeline.py
56. amb3r/model_zoo.py (DA3, VGGT-Omega wrappers): https://github.com/HengyiWang/amb3r/blob/main/amb3r/model_zoo.py
57. demo.py (base-model demo, `max_images=4`): https://github.com/HengyiWang/amb3r/blob/main/demo.py
58. Issue #9 "Error when running git clone" (Git LFS, fixed): https://github.com/HengyiWang/amb3r/issues/9

Local project references:

- L1: `src/seattle_world/cameras.py` (OpenCV c2w, COLMAP frame, PINHOLE)
- L2: `models/manifest.json` (DUSt3R checkpoint in use)
- L3: `experiments/mask-pilot/`
- "context": project state supplied with the research brief
