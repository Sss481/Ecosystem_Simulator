﻿# include <Siv3D.hpp> // OpenSiv3D v0.6.11


// 変な書き方をしていても温かい目で見てください！


constexpr int species = 5; // 種の数
constexpr int numCreatures = 800; // 初期状態での生物の合計数
constexpr int columns = 6; // パッチの列数（縦線でいくつに区切られるか。1以上）
constexpr int rows = 3; // パッチの行数（横線でいくつに区切られるか。1以上）
constexpr double creatureSpeed = 0.3; // 生物の移動速度
constexpr double maxTurnAngle = 90_deg; // 生物が1秒ごとに行う方向転換の最大値
constexpr double creatureRadius = 3.5; // 生物の半径（見た目のみ）
constexpr int maxEnergy = 100; // 最大エネルギー。この量のエネルギーがたまったら分裂する。不利な場所で生きられる時間にも関わる
constexpr int metabolism = 5; // 常時消費するエネルギー
constexpr double deltaComp = 1.0; // 競争能力（competitiveness）の差をどれくらいにするか
constexpr double nicheDifference = 0.2; // ニッチの差をどれくらいにするか。0以上1以下
constexpr bool arithSeqComp = 0; // trueにすると競争能力が全パッチ共通かつ等差数列（arithmetic sequence）になる
constexpr bool invasionMode = 1; // trueにすると種０が強い種1匹に置き換えられた状態からスタート
constexpr int FPS = 30; // フレームレート
constexpr int csvWriteInterval = 1000; // 何秒ごとにCSVファイルにデータを書き込むか
namespace {
	const String csvName = U"a.csv"; // 保存するCSVファイルの名前
}

// 生物
struct Creature {
	double E = maxEnergy * Random(0.3, 0.7); // エネルギー。maxEnergyの0.3倍から0.7倍
	Vec2 position = RandomVec2(Scene::Rect()); // 画面内のランダムな位置に初期化
	Vec2 V = Vec2(creatureSpeed, 0).rotated(Random(0_deg, 360_deg)); // 速度ベクトル
};

// パッチ
struct Patch {
	double comp[species]; // 種ごとの競争能力
	int num[species] = { 0 }; // パッチ内の生物数を種ごとに数える

	Patch() {
		for (auto i : step(species)) {
			// compはmetabolism ± (deltaComp / 2)の範囲でランダム（一様乱数）
			comp[i] = Random(metabolism - (deltaComp / 2), metabolism + (deltaComp / 2));
		}
	}
};


void Main() {
	Window::Resize(800, 400); // ウィンドウのサイズを設定する
	Scene::SetBackground(Palette::Black);
	uint64 count = 0; // シミュレーション内部の経過時間
	int simulationSpeed = 0; // シミュレーション速度（倍速）の調節

	// 生物たちを生成
	Array<Creature> creatures[species]; // 静的配列とSiv3Dの動的配列Arrayを組み合わせた二次元配列
	for (auto i : step(species)) {
		creatures[i].reserve(int(1.2 * numCreatures / species));
		for (auto j : step(numCreatures / species))
			creatures[i] << Creature();
	}

	Patch patches[columns][rows];

	if (invasionMode) {
		creatures[0].clear();
		for (auto i : step(columns)) {
			for (auto j : step(rows)) {
				patches[i][j].comp[0] = metabolism + deltaComp;
			}
		}
		Creature newCreature;
		creatures[0] << newCreature;
	}

	if (arithSeqComp && species > 1) {
		for (auto i : step(columns)) {
			for (auto j : step(rows)) {
				for (auto k : step(species)) {
					patches[i][j].comp[k] = metabolism -
						deltaComp / 2 + k * deltaComp / (species - 1);
				}
			}
		}
	}

	CSV csv;

	// CSVにデータを書き込む
	csv.write(U"経過時間");
	for (auto j : step(species)) {
		csv.write(U"種{}"_fmt(j));
	}
	csv.newLine();

	//フレームレートを調節するためにstopwatchを使う
	Stopwatch stopwatch;
	stopwatch.start();

	while (System::Update()) {

		ClearPrint();

		// 右矢印キーでシミュレーション速度を上げる。Shiftを押していると一気に5、Ctrlを押していると一気に50
		if (KeyRight.down()) {
			simulationSpeed += 1 + 4 * KeyShift.pressed() + 49 * KeyControl.pressed();
		}

		// 左矢印キーでシミュレーション速度を下げる。Shiftを押していると一気に5、Ctrlを押していると一気に50
		if (KeyLeft.down()) {
			simulationSpeed -= 1 + 4 * KeyShift.pressed() + 49 * KeyControl.pressed();
			if (simulationSpeed < 0) simulationSpeed = 0;
		}

		// 下矢印キーでシミュレーション速度を0にする
		if (KeyDown.down()) {
			simulationSpeed = 0;
		}

		for (auto i_simulationStep : step(simulationSpeed)) {

			// CSVにデータを書き込む
			if (count % (FPS * csvWriteInterval) == 0) {
				csv.write(count / FPS);
				for (auto j : step(species)) {
					csv.write(creatures[j].size());
				}
				csv.newLine();
			}

			for (auto i : step(species)) {
				for (auto& creature : creatures[i]) {
					// 速度ベクトルを使って新しい位置に移動
					creature.position += creature.V;

					// 画面外に出て行かないようにする
					if (creature.position.x <= 0 || creature.position.x >= Scene::Width()) {
						creature.position -= creature.V;
						creature.V.x *= -1;
						creature.position.x = Clamp(creature.position.x, 0.0, double(Scene::Width()));
					}
					if (creature.position.y <= 0 || creature.position.y >= Scene::Height()) {
						creature.position -= creature.V;
						creature.V.y *= -1;
						creature.position.y = Clamp(creature.position.y, 0.0, double(Scene::Height()));
					}

					// 速度ベクトルの角度をランダムに少し変化させる
					if (count % FPS == 0) {
						creature.V.rotate(Random(-maxTurnAngle, maxTurnAngle));
					}

					// エネルギーを消費する
					creature.E -= metabolism;

					// この生物がいるパッチのnum[i]に＋１
					int posX = int(creature.position.x / Scene::Width() * columns);
					int posY = int(creature.position.y / Scene::Height() * rows);
					patches[posX][posY].num[i]++;
				}

				// エネルギーが貯まったら分裂する。
				Array<Creature> newCreatures;
				for (auto& creature : creatures[i]) {
					if (creature.E > maxEnergy) {
						Creature newCreature;
						newCreature.position = creature.position;
						// エネルギーをぴったり2等分にすると、多数の生物が同時に死んでしまって困る場合がある
						newCreature.E = creature.E * 0.45; 
						creature.E /= 2;
						newCreatures << newCreature;
					}
				}
				for (auto& newCreature : newCreatures) {
					creatures[i] << newCreature;
				}

				// エネルギーがなくなったら餓死
				creatures[i].remove_if([](Creature c) {return c.E <= 0; });
			}

			// エネルギーを得る
			for (auto i : step(species)) {
				// 競争の激しさを、ニッチの差を考慮して求める
				double competitionEffect[columns][rows] = { 0 };
				for (auto j : step(columns)) {
					for (auto k : step(rows)) {
						for (auto l : step(species)) {
							if (i == l)competitionEffect[j][k] += patches[j][k].num[l];
							else competitionEffect[j][k] += patches[j][k].num[l] * (1 - nicheDifference);
						}
					}
				}
				for (auto& creature : creatures[i]) {
					// 生物がどのパッチにいるか計算する
					int posX = int(creature.position.x / Scene::Width() * columns);
					int posY = int(creature.position.y / Scene::Height() * rows);
					// 初期状態の生物数を基準にして競争の激しさを補正する
					double a = numCreatures / (competitionEffect[posX][posY] * columns * rows);
					// そのパッチにおける競争能力と組み合わせてエネルギー獲得量を計算する
					creature.E += Min(patches[posX][posY].comp[i] * a, metabolism + 3 * deltaComp);
				}
			}

			// パッチ内の生物数のカウントを0に戻しておく
			for (auto i : step(columns)) {
				for (auto j : step(rows)) {
					for (auto k : step(species)) {
						patches[i][j].num[k] = 0;
					}
				}
			}

			count++;

		} // 倍速によって複数回やる処理はここまで。ここからは1フレームに1回だけやる処理

		// パッチの境界線を描画する
		for (int i = 1; i < columns; i++) {
			Line(i * Scene::Width() / columns, 0,
				 i * Scene::Width() / columns, Scene::Height()).draw(HSV(0, 0, 0.6));
		}
		for (int j = 1; j < rows; j++) {
			Line(0, j * Scene::Height() / rows,
					Scene::Width(), j * Scene::Height() / rows).draw(HSV(0, 0, 0.6));
		}

		// 生物を描画する
		for (auto i : step(species)) {
			for (auto& creature : creatures[i]) {
				Circle(creature.position, creatureRadius).draw(HSV(360.0 / species * i, 0.9, 0.7));
			}
		}

		Print(U"経過時間  {}秒"_fmt(count / FPS));
		// 倍速などの情報を表示。CtrlまたはShiftまたはZまたは上矢印キー。
		if (KeyShift.pressed() || KeyControl.pressed() || KeyZ.pressed() || KeyUp.pressed()) {
			Print(simulationSpeed, U"倍速");

			Rect(Scene::Width() - 12 * creatureRadius, 0,
				12 * creatureRadius, (species + 1) * 6 * creatureRadius).draw(Palette::Gray);
			for (auto i : step(species)) {
				Print(U"種{}の数  {}"_fmt(i, creatures[i].size()));
				// どの種が何色なのかわかるように、種０から順に凡例を表示する（画面右上）
				Circle(Scene::Width() - creatureRadius * 6, creatureRadius * 6 * (i + 1), creatureRadius * 2)
					.draw(HSV(360.0 / species * i, 1.0, 0.7));
			}

			// マウスカーソルがある位置のパッチのcomp一覧
			if (Cursor::PosF().x >= 0 && Cursor::PosF().x < Scene::Width()
				&& Cursor::PosF().y >= 0 && Cursor::PosF().y < Scene::Height()) {
				int cursorX = int(Cursor::PosF().x / Scene::Width() * columns);
				int cursorY = int(Cursor::PosF().y / Scene::Height() * rows);
				Print(patches[cursorX][cursorY].comp);
			}

			// フレームレート
			Print(Min(FPS, int(1.0 / stopwatch.sF())), U" FPS");

			if (arithSeqComp)Print(U"ArithSeqComp Mode");
			if (invasionMode)Print(U"Invasion Mode");
		}
			
		//フレームレートを調節するために待機する
		System::Sleep(1000.0 / FPS - stopwatch.msF());
		stopwatch.restart();
	}

	// 終了時にCSVファイルを保存する
	csv.save(csvName);
}