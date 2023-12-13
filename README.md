# Birthday Problem Collision Generator

This is an implementation of a k-dimensional birthday-problem solver, roughly based on the paper [A Generalized Birthday Problem](https://link.springer.com/content/pdf/10.1007/3-540-45708-9_19.pdf) by David Wagner. In its default configuration it solves the subset sum problem with addition modulo `2**256`, however it can be adapted to a variety of cryptanalysis applications.

<!-- TOC FOLLOWS -->
<!-- START OF TOC -->
* [Concept](#concept)
* [Build](#build)
* [Finding Collisions](#finding-collisions)
  * [Example Collisions](#example-collisions)
  * [Arbitrary Targets](#arbitrary-targets)
* [Algorithm](#algorithm)
  * [Generation](#generation)
  * [Negation](#negation)
  * [Merge](#merge)
  * [Collision Search](#collision-search)
  * [Success](#success)
* [Implementation Notes](#implementation-notes)
* [Other Implementations](#other-implementations)
* [Author](#author)
<!-- END OF TOC -->


## Concept

Suppose you are given an arbitrary 32 bytes, which we'll call the "target". Can you find a set of inputs that when hashed with SHA-256 and added together (mod `2**256`) equal this target value?

Normally the target value would be an [incremental hash](https://people.eecs.berkeley.edu/~daw/papers/inchash-cs06.pdf) of some particular set of values, and our objective would be to find a *different* set of values with the same hash. For this reason, we will refer to solving a target value as finding a "collision" (in fact, this program implements a preimage attack).

Efficient solutions to this rely on the well-known birthday problem, which trades off space for time. If we keep a collection of all hashes we generate and compare each new hash against this collection, the total number of comparisons performed grows quadratically.

However, reducing the work required by a square root is still not sufficient for finding 256-bit collisions. For that, we need to build a set of nested pools, each of which solves a partial collision (relying on the birthday effect). The most nested pool solves the least significant bits (because the carry bits only propagate in one direction), and then passes its partial collisions up to the next layer, and this continues until a full collision is found.

For the complexity analysis, refer to Wagner's paper above. Also see the papers [A New Paradigm for Collision-free Hashing: Incrementality at Reduced Cost](https://cseweb.ucsd.edu/~mihir/papers/inc-hash.pdf) and [Reviving the Idea of Incremental Cryptography for the Zettabyte era](https://eprint.iacr.org/2015/1028.pdf).

The rest of this document will focus on the implementation details which do not directly follow from the idealised algorithm description in the paper.


## Build

To compile on a Debian/Ubuntu system, run:

    git submodule update --init
    apt install -y build-essential g++ libtbb-dev
    make


## Finding Collisions

To run, you must pass `bday` a directory where it will put temporary files. Warning: these can get very large! There is also an optional `target` parameter. If omitted, the target is considered to be all 0 bytes (a "null collision").

    ./bday <path-to-memdir> [target]

Various environment variables can be set as well. For example, `NUMSTAGES` can be used to exit early with a partial collision. Each stage solves 4 bytes, starting from the least-signficant digits. For example:

    $ NUMSTAGES=3 ./bday /mnt/

    <... a lot of logging output ...>

    7dabacf3ad8ce12247ca21225852d26f092c70906f2d82a86c98e811434bfec6 (34060500)
    775fa39deb0468cd9f5b9a35c2c81cc52dee3bdde0201c3878b20f30db8c277c (110980006)
    7ca24136d31db21f7da70795b2df74ac20772a771b41d2715a09993c312ad83a (157113646)
    986ed00f79bf1684932c6732e2557c7d0bc28843b89b2061e90991d301200afa (158028712)
    6e37569b5e1c567d1a131451c95364d2b77bfb2464935a69b4d8cc38fedff506 (350928731)
    dd2acabb6a9b702aa8eb3038ad3af0e64aff2e79f00aefb7c9b070d42473d884 (412495621)
    1d662d29b89023846a79e880c84ba54805b8fab76b2adbfa081408b7ced527c6 (479503101)
    40a6811294e8d6267707acb6a2f186832d9e7bb61d0c4830510497e9bcb4013a (658212370)

The output lines consist of the initial seed values in parentheses, preceded by their SHA-256 hashes. There is a script called `add-input.pl` that adds up all the hashes on its standard input, one per line:

    $ for i in 34060500 110980006 157113646 158028712 \
               350928731 412495621 479503101 658212370 ; \
               do echo -n $i | sha256sum; done | ./add-input.pl

    b38b316afb9ed2e69c7903e2921b61e19926ff35000000000000000000000000

As expected, the right-most `3*4 = 12` bytes (24 hex characters) are zeros.


### Example Collisions

Here is a shell script that verifies a full null collision found by `bday`:

    ITEMS="3829559 5851718 125485973 129113872 131184248 159067254 174338733 180827547 190630699 193623858 198940404 204827716 218022895 223069199 234572579 244243376 245619720 255021490 292365890 315996717 320808354 328741565 335829294 347087030 371731340 386689852 408403327 408670572 413114674 431491093 439351389 443061498 449816823 492748034 565562484 596262363 599006900 620018890 621221667 626996384 630171641 731680303 732463898 773015067 802142086 855832375 885874778 890883871 897068819 900042222 900707962 904865492 910930319 914159545 927929432 937734642 947943078 961483530 990537361 995515143 1047418335 1087956806 1103492311 1128482937 1141986332 1142094578 1175120618 1186496472 1204734527 1241298802 1245272757 1340445099 1371844537 1378882558 1395408799 1397578841 1399246650 1406137023 1418823996 1439067578 1463337119 1480387204 1514829675 1520593220 1526881266 1535602502 1542311326 1572546591 1578639044 1591068177 1611844158 1630753798 1637837440 1645045380 1672145579 1683443190 1708269502 1718685926 1721975900 1727842680 1773334996 1779889345 1818607647 1858262026 1859109456 1878311158 1950069892 1973946495 1996656826 2007547580 2043348095 2087399745 2145964213 2149637688 2207284034 2212894908 2224712647 2240856587 2286608408 2323359234 2333381183 2345947088 2374288603 2379284858 2403897316 2407485305 2479797327 2480800455 2512723234 2518645202 2538813569 2543210393 2578552064 2643644450 2687230816 2703106079 2825407233 2940290823 2979939957 2985410210 3093283462 3108915142 3125045929 3145422331 3153668123 3203227077 3211497656 3246152807 3264430380 3284197625 3297738236 3307908122 3326054638 3353839714 3356508522 3399846996 3448291732 3506595855 3563411335 3694200862 3724648852 3729057152 3755261119 3773731435 3787649778 3798130795 3884705650 3913912548 3914017603 3948206797 3987386701 3990619319 3999946544 4074103368 4269081691 4344359543 4501792950 4522242700 4564820491 4576498805 4683346071 4763175622 5030954463 5120259921 5160032591 5216092381 5310498251 5352928529 5400020453 5710110729 5784385034 5924949389 6058528498 6066190272 6117948520 6119682969 6153190376 6182838858 6189390279 6197899782 6218328556 6235568047 6377337987 6417910711 6465471695 6466693614 6481873355 6566761641 6584662484 6588082588 6693735280 6733686841 6877128186 6888658361 6904291444 6922830528 6950330551 7095122966 7098544063 7159299926 7164535101 7433160199 7626523072 8152034261 8369251875 8471243626 8514178001 8604254822 8781856713 8911273626 9404739671 10011540915 10251836537 10374282912 10513500676 11070075123 11423492346 11811592657 11853886610 12054124024 12140049342 12237391053 12241590413 12308063068 12378453836 12644026830 12667664189 12693825839 13152399918 13224320689 13566158237 13586305611 14087949733 14493233972 14715256459 15360349590"
    for i in $ITEMS; do echo -n $i | sha256sum; done | ./add-input.pl

You will see this output:

    0000000000000000000000000000000000000000000000000000000000000000

The log output for the run that found this collision is in `results/collision1.txt`.

Generating this took 14.5 hours and required about 1.5 TB of disk space. I used a VM allocated with the following resources:

* 8 cores of an AMD 1950X threadripper
* 60 GB of ECC DDR4 memory, at 2133 MHz
* 2 TB storage on a WD SN850X NVME drive

I estimate the time could be reduced around 25-50% with additional code optimisations and tuning. Providing additional resources (especially RAM and a more modern CPU) would reduce it further still.

Here is another shell script that demonstrates a collision for an arbitrary target ("dead beef"x8):

    ITEMS="68860700 125546823 527415655 581119796 585818715 906655011 611425043 849482092 1025089583 1272172806 1315315431 1380745119 1417647444 1461853274 701268386 741409871 903273840 1077019465 1208356150 1274155959 1491890421 1617882199 1626979488 1777972750 1807116218 2500673465 2511505627 2665039274 2839410082 2889202474 546802314 549952591 1115064128 1590835347 1764463766 2064762095 2173279116 2420581490 2463082843 2528697229 2644733984 2713000676 2942293980 3118120748 3145757295 3296920147 3585621838 3607498839 3654345310 3665903287 3786600615 3859544301 4079251264 4495576447 4777637009 5273540124 5395599727 5411339191 5449020218 6588486634 7141795770 7308869714 799151000 877291507 906766652 1153169494 1183591780 1229451978 1278040798 1990690009 2055663660 2237479925 2240549740 2293127067 2485607874 2603265824 2615624223 2732950944 2781222643 3059386357 3085252749 3370998199 3382289731 3418935527 3655631150 3719920748 3729320136 3737356806 3754295700 3841886337 3887285816 4092646689 4123569467 4151585504 4213899941 4295312236 4383401584 4401459356 4931446787 5123590890 5811397318 5998404470 6025980050 6053677043 6357532994 7193166624 7206798745 7899900041 8170683647 8499631924 8785297381 8913014310 9335441589 9523309414 9539422803 11271653713 11302406456 11326306222 11655836758 11688038343 11916628235 12531307346 12569972205 13208767580 13461960849 13777744612 526510754 535682065 596903961 615173978 725463934 733502470 848136276 857956912 921678687 1075603406 1130614109 1131553642 1294354665 1369380412 1483371879 1635970363 1663745897 1697573103 1844126036 2013567617 2150907750 2169341864 2191846654 2445348450 2455242220 2567366639 2651015017 2658686563 2701258743 2745112783 2763414139 2818109903 2830056970 2850189369 2991313626 2999279932 3005607787 3008364437 3050817306 3104492539 3155754441 3197451263 3206990015 3222382490 3256673683 3305731295 3400718646 3435585296 3459643993 3521003519 3541855153 3565684160 3601259798 3614708020 3627304062 3662354176 3703429843 3711385915 3755485778 3791391004 3901920810 3912214321 3930107875 3966965459 4141024238 4163110361 4168440692 4205842137 4234180710 4238770868 4257854370 4259445152 4309572582 4344187340 4406618791 4419594963 4436367565 4441278488 4454598777 4816713998 5054067212 5269135774 5546320914 5626535650 6112252518 6513810198 6591642205 7060837631 7157101081 7316149402 7698045080 7861190506 8221972989 8273186113 8679921681 8747152932 8820990176 9365578469 9432605024 10040169385 10065251310 10089374808 10126830990 10164048678 10573248109 10817208205 11398020111 12014136720 12525842080 12997660191 13072526216 13273554992 13355632538 14262452357 14323590701 14584815892 14622578264 14721469398 14884391383 15467522772 19294252668 20434124869 20481023069 20581334225 20792908525 21151812213 21983465021 21989292808 505102519 581398151 719540178 821947181 840173791 905984731 1012568518 1049835102 1050416078 1055073557 1195362676 1216621993 1330345975 1388319202 1401312519 1422644025 1425685084 1463506883 1476735558 1575396526 1598823177 1599940634 1642998139 1660674797 1726024728 1747783560 1784625957 1785123561 1795424477 1821179156 1834305131 1865252779 1881198086 1897193950 2021617914 2033140313 2041209901 2044682946 2118264136 2137013584 2154135115 2173778934 2204459235 2244711699 2267917027 2272497657 2278860202 2333871248 2334393502 2494341607 2522569065 2528357034 2538744924 2542595528 2621276842 2697801880 2794376464 2835570908 2837600593 2849466277 2850734358 2874539782 2910370772 3007769661 3018534895 3042878218 3096969272 3106467345 3132567185 3213291023 3220037813 3250312909 3304001532 3331834947 3346732071 3366543571 3414314798 3445109178 3488794684 3510557563 3529580300 3532827118 3547299194 3563391737 3566678339 3567164890 3578798381 3583053014 3591977348 3601045365 3622996697 3639731794 3667509563 3686734580 3691206703 3701745321 3721788537 3744124896 3759859756 3797807127 3800131994 3821598671 3824242201 3848867170 3860017262 3885605096 3902293739 3907495467 3914366240 3928009131 4003199402 4014735583 4038894282 4067434537 4072362806 4074807487 4090592672 4090639510 4104351459 4118296863 4126283152 4131810596 4142301141 4207854668 4221727670 4236499337 4245534477 4280997124 4289779091 4303593763 4316002163 4319456334 4326574184 4334619440 4378319687 4404662672 4406092542 4418155390 4429921096 4479917374 4488506036 4492641777 4496298727 4497684784 4516393172 4531605085 4671435748 4672195767 4696991750 4950500404 5015561201 5209134074 5236696319 5481132952 5616827793 5635691387 5656784997 5908771637 5956842695 6210627410 6319623677 6331882583 6517027223 6625535118 6698915749 6702036587 6806515575 6860422033 7017121846 7017142853 7183101620 7217602539 7293852515 7399830901 7697814801 7960083392 8060669257 8078732215 8298081250 8328283554 8414204264 8640417331 8673146273 8722515880 8781871368 8890032005 8976358642 9009433527 9344053241 9347443894 9443404020 9451571427 9647401898 9754575909 9815709880 9855731646 10023475577 10178836484 10450783620 10690925824 10870810531 11303868102 11334909386 12155251312 12379282242 12436290631 12776339079 13061104884 13360782788 13523603381 13591244641 13887273678 14202819582 14433795913 14507841019 14601659292 14647337523 14663682463 15302718546 15350661470 15568987726 15753939340 15948046694 16241888390 16355292213 16488799294 16724563024 17641866789 17896383396 19141036469 19420849641 20790178810 21044957744 21592845529 22102403719 22196611502 22838668590 22872867053 22921426290 23343010707 24345715317 24371232688 24982455440 25337391171 25425294218 25914608849 26011805291 26428158563 27145022127 27618115866 28030198134 28192525196 28331525712 31344723360 33190317768 33421514042"
    for i in $ITEMS; do echo -n $i | sha256sum; done | ./add-input.pl

The output:

    deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef

Generating this took 28 hours and also required about 1.5 TB of disk space on the same system as above.


### Arbitrary Targets

As mentioned above, `bday` takes an optional `target` parameter. This is the desired collision value, and its length must be a multiple of 4 bytes (8 hex digits). If smaller than 32 bytes (64 hex digits), then a partial collision will be found.

Although Wagner's paper considers how to generate arbitrary targets for XOR, it does not do so for addition. Due to the propagation of the carry bit, solving this is more complicated. `bday` accomplishes this with a method called "staggered solutions".

First of all, a partial collision in the first stage can trivially be found by offsetting the negated collision value by the target (see the Algorithm section below):

    $ ./bday /mnt/dat/ aaaaaaaa
    4271b5ec5517c9c95335de190149a254bfaeba7691018bf60661b190fca0aaaa (23056959)
    da07d13e14f365ef558eb32dcad81ca27e52a03c47a87ef5d0a3f56fae0a0000 (8975480)

If you pipe these values to `add-input.pl`, they sum to:

    1c79872a6a0b2fb8a8c49146cc21bef73e015ab2d8aa0aebd705a700aaaaaaaa

The trivial method will no longer work when multiple stages are involved. To understand why, consider that the sum of two hashes that each end in `N` 0 bytes will *also* end in (at least) `N` 0 bytes. If the hashes end in any other values, adding them together will change the ending values. Furthermore, carry bits from the addition may propagate from the previous stages into the current stage. This coupling prevents stages from being worked on independently.

Suppose we want a partial collision ending in `bbbbbbbbaaaaaaaa`. The staggered solution approach is to solve a suffix and then preserve it by finding other collisions that end in trailing 0 bytes.

If we start with our `aaaaaaaa` partial collision above, you might try to find a partial collision for `bbbbbbbb00000000`. However, this will not work because the 4 bytes to the left of the `aaaaaaaa` are `d705a700`, and so adding `bbbbbbbb` to them would result in an undesired value. The trick is to take advantage of the ordering of the staggered solutions. When we solved the first stage (rightmost 4 bytes), we did not care about the bytes to the left, meaning they are arbitrary. However, *after* we solve the first stage, we can take these arbitrary bytes and use them to create an adjusted target value for the next stage, by subtracting them from our actual target (mod `2**32`): 

    0xbbbbbbbb - 0xd705a700 (mod 0x100000000) = 0xe4b614bb

This indicates the target we should solve for in the next staggered step:

    $ ./bday /mnt/dat/ e4b614bb00000000
    f0a9eefd0a84e3f9b0cbbe00244c08c2a892c16b9ba45ef10e851fb28e5468c5 (42589910)
    dbeefe160b9672a2990deea228445c681e1d0bd8eccadb19eb46e25071ab973b (31461223)
    20bf9be670ba07701fe1be0c655d3824b50ba4c86b5ed1919806b4774c15bff1 (34312154)
    9a888df92ca62cc62df77ab12d3b349c068bbe01dd60cfc252e35e40b3ea400f (30992687)

When these 4 hashes are summed up together with the 2 hashes from stage 1, we get the following:

    a45a9e1d1d86ba8b407776a6ab4a90e2c0488ac1a9d8e64abbbbbbbbaaaaaaaa

This procedure can be repeated for each stage until the full target hash is achieved.

For example, here are the targets that were used to solve the "dead beef" collision above:

    deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef
    deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef09328c5c00000000
    deadbeefdeadbeefdeadbeefdeadbeefdeadbeef816d07db0000000000000000
    deadbeefdeadbeefdeadbeefdeadbeefe1c51d2b000000000000000000000000
    deadbeefdeadbeefdeadbeef2f5f02a300000000000000000000000000000000
    deadbeefdeadbeef7126253d0000000000000000000000000000000000000000
    deadbeef275808d6000000000000000000000000000000000000000000000000
    8abf06f000000000000000000000000000000000000000000000000000000000

Note that after the first staggered solution, subsequent solutions will be solving multiple partial collisions ending in 0 bytes. Rather than throw out and redo the work that was done to find these partial collisions, `bday` keeps them around while advancing stages. It only needs to redo the stages containing the adjusted target values.



## Algorithm

A multi-stage pipeline of partially colliding hashes is maintained. Each stage contains 3 vectors of hashes:

* `inbox`: An unsorted vector of hashes (or sums of hashes) waiting to be processed
* `big`: A sorted vector of all the processed hashes
* `found`: A vector of all partial collisions found

### Generation

When the application first starts, it generates `batchSize` (by default 500 million) hashes, using sequentially increasing "seed" values, which are integers stringified in ASCII decimal. In a real-world application, you would implement a different function for rendering a `uint64_t` into some acceptable pre-image value.

Each hash in the batch is then byte-wise reversed, because we will work on finding partial collisions at the least-significant bits first. The hashes will remain reversed until the very end, at solution display-time.

Each hash is put into a 40-byte `Elem` struct, along with the original seed value. The batch of `Elem`s is then added to the inbox of the first stage (stage `0`).

### Negation

When a stage runs, if it has items in its inbox, it will create an internal copy of the inbox, called `inboxNegs`. Then for each elem in `inboxNegs` it will in-place compute the negation of its hash, which in twos-complement is the bit-wise complement plus one. It will then add the target value, mod `2**256` to the negated hash.

Both the `inbox` and `inboxNegs` vectors are then lexically sorted.

To solve different classes of problems, the negation and/or addition routines would need to be altered. For instance, to solve for moduli other than `2**256` (see Wagner's paper).

### Merge

The `inbox` vector is merged into `big`, which increases its size. Since both inputs are sorted, the resulting `big` vector is also sorted.

### Collision Search

The `big` and `inboxNegs` vectors are then traversed. If any items share the first `4 * (numStage + 1)` bytes, then a partial collision has been found. The item from the `inboxNegs` is negated and target is added to it, which recovers the original `inbox` item. This item and the matching item from `big` are added together, resulting in a 32 byte value that shares a prefix with the target value.

All partial collisions are assigned an `id` value that is internal to the stage they were generated by. New `Elem` values that contain these `id`s and the partial collisions are created, and these are appended onto the next stage's `inbox`.

Additionally, each new `id` is collected into a `FoundElem` struct along with the two parent `id`s of the combining hashes, and this is pushed onto the `found` vector.

### Success

Once the final stage finds a collision, a solution has been discovered. Both of the parent `id` values of the hashes that resulted in the collision are looked up by searching the previous stage's `found` vector using a binary search, and this proceeds recursively for each of those `id`s until we have a set of the original seeds, which represents the inputs that resulted in the full collision.


## Implementation Notes

In the idealised algorithm from Wagner's paper, there is a possibility that the computation will fail. In fact, it is parameterised such that the expected number of solutions found will be `1`. [This paper](https://people.eecs.berkeley.edu/~sinclair/ktree.pdf) analyses the failure probability in more detail, suggests it can be as high as 3/4, and provides a re-parameterisation to reduce this.

In our implementation, the algorithm will keep going until it finds a solution. It does this by keeping enough partial collisions in the earlier stages to allow us to perpetually pump new partial collisions through to the downstream stages. Note that after collecting `MERGELIMIT` hashes, a stage will no longer merge new hashes from its `inbox` into its `big` vector. This keeps the disk usage of the `big` vectors bounded at `NUMSTAGES * (MERGELIMIT + BATCHSIZE - 1) * 40` bytes. The `found` vectors are not bounded, but seem to take up approximately as much space as `big` at solution-time. The `found` vectors could be stored on a separate slower storage (they are only needed at the very end).

Another reason for `MERGELIMIT` is that once the `big` vector becomes large enough, merging new values into it becomes the dominant time and IO cost for processing a stage. At some point, the compounding benefit of more partial collisions will no longer outweigh this cost. A reasonable `MERGELIMIT` value was determined experimentally (this could be optimised further).

The various vectors are not allocated in anonymous memory, but rather in memory mapped files within the `memdir` directory. This has several purposes:

* Most systems will not have the necessary amount of swap configured
* By monitoring the cache status of these files, we get a lot of visibility into how the algorithm functions. For example: `watch vmtouch -v /mnt/`
* The memory can be backed by a dedicated/special-purpose block device
  * On systems that *do* have enough physical memory, we can put them in hugetlbfs
* In case of a crash or interruption, the partial collisions stored in the `big` vectors may be useful for resuming the computation

In most circumstances, RAM will be the full-system bottleneck since it limits the `BATCHSIZE` parameter. `BATCHSIZE * 40` should be approximately 1/3 of memory. The larger this is, the more comparisons can be performed per merge. The parallel sort implementation used by my compiler (g++ 10.2.1, TBB 2020.3-1) seems to not be a fully in-place sort. If it was, batches could instead be parameterised at 1/2 of memory (more investigation is needed here).

Access to `inbox` is random, which is why we need to ensure it can always fit in RAM. However, all access to the `big` and `found` vectors is sequential, so a spinning rust hard disk may actually provide adequate performance. That said, I have only tested on NVME SSDs.

I have inspected the disassembly of the core loops and many of them use SIMD instructions. The `add` routine does not -- this would be an obvious candidate for optimisation, however at best this can be a modest gain since this part of the pipeline is not a substantial bottleneck.

All steps of the processing within a stage are done using multiple parallel threads except for the merge and collision search. The collision search could be done in parallel by binary searching for suitable thread starting points. The merge could also be parallelised at the expense of larger intermediate RAM usage. Stages themselves could be worked on in parallel, however this would cause RAM and IO contention -- I believe that using full available RAM for the batch sizes is more effective. If you had multiple computers (or NUMA nodes), they could compute partial collisions independently and then send them to a hub to be merged: the `FoundElem` struct would have to be modified to track the source computer (or redundantly embed the seeds).

With long-running cryptographic jobs it is best to use ECC memory, since a single bit flip in the page cache could destroy hours or days of work-product.


## Other Implementations

There are very few public implementations of the Wagner attack. Here are the ones I know about:

* Bernstein et al (2009): [code](http://www.polycephaly.org/projects/fsbday/), [paper](https://eprint.iacr.org/2009/292)
* Adam Gibson (2021): [code](https://github.com/AdamISZ/wagnerexample), [blog](https://reyify.com/blog/avoiding-wagnerian-tragedies)
* Conduition (2023): [code](https://github.com/conduition/wagner), [blog](https://conduition.io/cryptography/wagner/)

If you know of any more, please [let me know](https://hoytech.com/about).


## Author

Doug Hoyte, 2023
